/* astro.c - the ASTRONAUT health console (keyboard only).
 * A non-blocking state machine: astro_key() handles one key event,
 * astro_tick() is called every main-loop pass (PIT time), astro_draw()
 * repaints only when something changed. Nothing here ever waits. */
#include "astro.h"
#include "tmr.h"
#include "vigil.h"

void gfx_fill_rect(unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned int color);
void gfx_draw_string(unsigned int x, unsigned int y, const char *s, unsigned int fg, unsigned int bg);
void gfx_draw_string_scaled(unsigned int x, unsigned int y, const char *s,
                            unsigned int fg, unsigned int bg, unsigned int scale);
unsigned int timer_ticks(void);
int serial_write(unsigned short com, char *buf, unsigned int len);
unsigned int strlen(const char *s);
void sensor_feed(char c);

extern unsigned int sensor_hr, sensor_spo2, sensor_temp, sensor_bp, sensor_sleep;

#define C_BG     0x00101820u
#define C_HDR    0x000A121Bu
#define C_PANEL  0x00162231u
#define C_BOX    0x000B141Eu
#define C_LINE   0x002C4560u
#define C_DIM    0x0080A0C0u
#define C_TXT    0x00FFFFFFu
#define C_ACC    0x0040C8FFu
#define C_AMBER  0x00FFBF00u
#define C_FAULT  0x00FF4040u
#define C_OK     0x0000FF00u

#define C_GRN_BG 0x00007A2Au
#define C_WAIT   0x00502020u

/* ===== DEMO VALUES (one place) =====
 * Every channel is compared with the person's OWN baseline.
 * These percentages are demo values, not clinical limits. */
#define AMBER_PCT        10      /* rolling median 10% worse than own baseline -> AMBER */
#define RED_PCT          25      /* 25% worse -> RED */
#define BASE_N_CHECKIN    5      /* baseline = median of the first 5 check-in entries */
#define BASE_N_TEST       3      /* baseline = median of the first 3 test sessions */
#define ROLL_N            3      /* status uses the median of the last 3 entries */
#define CHECKIN_INTERVAL_TICKS 12000u   /* demo reminder: 120 s of uptime (100 ticks = 1 s) */
#define REACT_TRIALS      5
#define MATH_QUESTIONS    5
#define MEM_MAX           12

enum { CH_SUGAR, CH_MOOD, CH_REACT, CH_MEM, CH_MATH, NCH };
#define HIST 8                   /* in-RAM history per channel; lost on reboot */

typedef struct {
    const char *name, *label, *unit, *tag, *phrase;
    int base_n;
    int dir;                     /* 0 = either way is worse, 1 = higher is worse, 2 = lower is worse */
} chcfg_t;

static const chcfg_t chcfg[NCH] = {
    { "sugar",    "Blood sugar", "mg/dL", "ENTERED", "Blood sugar differs from your baseline",   BASE_N_CHECKIN, 0 },
    { "mood",     "Mood",        "/5",    "ENTERED", "Mood lower than your baseline",            BASE_N_CHECKIN, 2 },
    { "reaction", "Reaction",    "ms",    "MEASURED","Reaction slower than your baseline",       BASE_N_TEST,    1 },
    { "memory",   "Memory",      "steps", "MEASURED","Memory score below your baseline",         BASE_N_TEST,    2 },
    { "math",     "Mental math", "/5",    "MEASURED","Math score below your baseline",           BASE_N_TEST,    2 },
};

/* all values live in TMR storage: 3 copies, voted on read, repaired by the scrubber */
static tmr_u32      ring[NCH][HIST];
static tmr_u32      base_t[NCH];
static tmr_u32      best_t[NCH];
static unsigned int cnt[NCH];            /* entries ever saved (history position) */
static unsigned int sess[NCH];           /* sessions/entries saved this boot */
static int          base_set[NCH];
static unsigned int last_checkin = 0;    /* PIT tick of last check-in (0 = boot) */
static unsigned int a_inj = 0, a_fix = 0;
static int          inj_next = 0, inj_copy = 0;
static int          main_dirty = 0;
static int          demo_active = 0;    /* 1 after the SIMULATED demo history was loaded */
static int          vit_sim = 0;        /* 0 = vitals typed in by the astronaut, 1 = serial feed (simulator) */
static unsigned int vit_idx = 0;        /* which vital is being typed (0..4) */
static unsigned int vit_val[5];         /* last value typed for each vital (0 = none yet) */
static unsigned int vit_cnt[5];         /* entries typed this boot */
static int          loading = 0;        /* 1 while the demo history is being written */

typedef enum {
    S_CLOSED, S_MENU, S_GUIDE, S_SUGAR, S_MOOD, S_CI_DONE,
    S_R_IDLE, S_R_WAIT, S_R_GO, S_R_MSG, S_R_DONE,
    S_M_IDLE, S_M_SHOW, S_M_INPUT, S_M_DONE,
    S_Q_IDLE, S_Q, S_Q_DONE,
    S_VIT, S_VIT_DONE
} astate_t;

/* ---- small text helpers (no printf in this kernel) ---- */
static void put_num(char *dst, unsigned int *n, unsigned int v)
{
    char t[11]; int i = 10;
    t[i] = 0;
    do { t[--i] = '0' + (v % 10); v /= 10; } while (v);
    while (t[i]) dst[(*n)++] = t[i++];
    dst[*n] = 0;
}

static void put_str(char *dst, unsigned int *n, const char *s)
{
    while (*s) dst[(*n)++] = *s++;
    dst[*n] = 0;
}

/* ---- serial telemetry (own tiny helpers; same style as the existing lines) ---- */
static void ser_str(const char *x) { serial_write(0x3F8, (char *)x, strlen(x)); }
static void ser_num(unsigned int v)
{
    char b[12]; unsigned int n = 0;
    b[0] = 0; put_num(b, &n, v); ser_str(b);
}

/* ---- channel logic: store, baseline, status (reuses tmr.c; no second voter) ---- */
static unsigned int chan_get(int ch, unsigned int k)      /* k-th newest entry (0 = latest) */
{
    return tmr_read(&ring[ch][(cnt[ch] - 1 - k) % HIST], 0);
}

static void sort_u(unsigned int *a, int n)
{
    for (int i = 1; i < n; i++) {
        unsigned int k = a[i]; int j = i - 1;
        while (j >= 0 && a[j] > k) { a[j + 1] = a[j]; j--; }
        a[j + 1] = k;
    }
}

/* -1 = building baseline, 0 = GREEN, 1 = AMBER, 2 = RED */
static int chan_level(int ch)
{
    if (cnt[ch] < (unsigned int)chcfg[ch].base_n || !base_set[ch]) return -1;
    int b = (int)tmr_read(&base_t[ch], 0);
    unsigned int t[ROLL_N];
    for (int k = 0; k < ROLL_N; k++) t[k] = chan_get(ch, (unsigned int)k);
    sort_u(t, ROLL_N);
    int med = (int)t[ROLL_N / 2];
    int d = med - b;
    int bad = (chcfg[ch].dir == 0) ? (d < 0 ? -d : d) : (chcfg[ch].dir == 1 ? d : -d);
    if (b <= 0 || bad <= 0) return 0;
    if (bad * 100 >= b * RED_PCT) return 2;
    if (bad * 100 >= b * AMBER_PCT) return 1;
    return 0;
}

static const char *level_word(int l)
{
    return l < 0 ? "BASELINE" : l == 0 ? "GREEN" : l == 1 ? "AMBER" : "RED";
}

static unsigned int level_col(int l)
{
    return l < 0 ? C_DIM : l == 0 ? C_OK : l == 1 ? C_AMBER : C_FAULT;
}

static void chan_add(int ch, unsigned int v)
{
    tmr_write(&ring[ch][cnt[ch] % HIST], v);
    cnt[ch]++;
    sess[ch]++;
    if (cnt[ch] == (unsigned int)chcfg[ch].base_n) {         /* baseline = median of the first N entries */
        unsigned int t[BASE_N_CHECKIN];
        for (int k = 0; k < chcfg[ch].base_n; k++) t[k] = chan_get(ch, (unsigned int)k);
        sort_u(t, chcfg[ch].base_n);
        tmr_write(&base_t[ch], t[chcfg[ch].base_n / 2]);
        base_set[ch] = 1;
    }
    /* best value: lowest reaction time; highest for the others (tests only) */
    if (ch >= CH_REACT) {
        unsigned int old = tmr_read(&best_t[ch], 0);
        int better = (sess[ch] == 1) || (ch == CH_REACT ? v < old : v > old);
        if (better) tmr_write(&best_t[ch], v);
    }
    int l = chan_level(ch);
    ser_str(ch < CH_REACT ? "CHK " : "COG ");
    ser_str(chcfg[ch].name); ser_str(" "); ser_num(v); ser_str(" ");
    ser_str(level_word(l));
    if (loading) ser_str(" SIM");           /* demo history is labelled on the wire too */
    ser_str("\n");
    main_dirty = 1;
}

/* one-line guidance for the main screen / console: first flagged channel after rotation */
static int attn_build(unsigned int now, char *out, unsigned int cap, int *level)
{
    int flagged[NCH], nf = 0;
    for (int ch = 0; ch < NCH; ch++)
        if (chan_level(ch) >= 1) flagged[nf++] = ch;
    if (nf == 0) return 0;
    int ch = flagged[(now / 300) % (unsigned int)nf];
    int l = chan_level(ch);
    unsigned int n = 0;
    out[0] = 0;
    put_str(out, &n, chcfg[ch].phrase);
    put_str(out, &n, ". ");
    put_str(out, &n, l == 2 ? "Report to the flight surgeon now." : "Rest, repeat, report if it persists.");
    if (n >= cap) out[cap - 1] = 0;
    if (level) *level = l;
    return nf;
}

/* ---- state ---- */
static astate_t st = S_CLOSED;
static int dirty = 0;               /* console needs a full repaint */
static int guide_page = 0;
static unsigned int last_strip = 0;
static unsigned int rng = 12345;
static char ibuf[8];                /* typed digits */
static unsigned int ilen = 0;
static unsigned int r_trial = 0, r_wait_until = 0, r_go_tick = 0;
static unsigned int r_ms[REACT_TRIALS];
static unsigned int r_result = 0;
static int          r_early = 0;
static int          prev_due = 0;
static unsigned int prev_rot = 0;
static unsigned int sugar_saved = 0;
/* memory test */
static unsigned char m_seq[MEM_MAX];
static unsigned int  m_len = 1, m_pos = 0, m_show_i = 0, m_until = 0, m_longest = 0;
static int           m_lit = -1;
/* mental math */
static unsigned int  q_i = 0, q_a = 0, q_b = 0, q_score = 0;
static char          q_op = '+';
static int           q_last = 0;            /* 0 none, 1 correct, 2 wrong */
static unsigned int  q_last_ans = 0;

static unsigned int rnd(void)       /* LCG, seeded from PIT ticks at test start */
{
    rng = rng * 1103515245u + 12345u;
    return rng >> 16;
}

/* ---- drawing ---- */
static void draw_header(void)
{
    gfx_fill_rect(0, 0, 800, 88, C_HDR);
    gfx_fill_rect(0, 88, 800, 2, C_ACC);
    gfx_draw_string_scaled(100, 16, "ASTRO CONSOLE", C_TXT, C_HDR, 3);
    gfx_draw_string(100, 66, "A1-37 VIGIL  |  your own health check  |  keyboard only", C_DIM, C_HDR);
    gfx_draw_string(8, 70, "HEARTBEAT", C_DIM, C_HDR);
    gfx_fill_rect(508, 20, 280, 48, C_AMBER);
    if (vit_sim) {
        gfx_draw_string_scaled(512, 28, "VITALS: SIMULATED", C_BG, C_AMBER, 2);
    } else {
        gfx_draw_string_scaled(512, 21, "VITALS: ENTERED", C_BG, C_AMBER, 2);
        gfx_draw_string(512, 51, "BY THE ASTRONAUT", C_BG, C_AMBER);
    }
}

static void fmt3(char *out, unsigned int v)
{
    if (v > 999) { out[0] = out[1] = out[2] = '?'; out[3] = 0; return; }
    out[0] = '0' + v / 100; out[1] = '0' + (v / 10) % 10; out[2] = '0' + v % 10; out[3] = 0;
}

static void draw_strip(void)                   /* the 5 SIMULATED vitals, refreshed once per second */
{
    char s[100];
    unsigned int n = 0;
    char t[4];
    gfx_fill_rect(0, 94, 800, 22, C_BG);
    s[0] = 0;
    put_str(s, &n, vit_sim ? "SIMULATED VITALS  HR " : "VITALS (ENTERED)  HR "); fmt3(t, sensor_hr); put_str(s, &n, t);
    put_str(s, &n, "  SPO2 "); fmt3(t, sensor_spo2); put_str(s, &n, t);
    put_str(s, &n, "  BP "); fmt3(t, sensor_bp); put_str(s, &n, t);
    put_str(s, &n, "  TEMP "); put_num(s, &n, sensor_temp / 10); put_str(s, &n, ".");
    put_num(s, &n, sensor_temp % 10);
    put_str(s, &n, "  SLEEP "); put_num(s, &n, sensor_sleep / 10); put_str(s, &n, ".");
    put_num(s, &n, sensor_sleep % 10);
    gfx_draw_string(20, 98, s, C_AMBER, C_BG);
}

static void draw_footer(const char *hint)
{
    gfx_fill_rect(0, 570, 800, 30, C_BG);
    gfx_draw_string(20, 576, hint, C_DIM, C_BG);
    gfx_draw_string(440, 576, "General information, not medical advice.", C_DIM, C_BG);
}

/* table of the 5 astronaut channels: value, status colour, bar strip, source */
static void draw_table(unsigned int y0, int from, int to)
{
    gfx_draw_string(20,  y0, "INDICATOR", C_DIM, C_BG);
    gfx_draw_string(124, y0, "LATEST", C_DIM, C_BG);
    gfx_draw_string(232, y0, "STATUS vs YOUR OWN BASELINE", C_DIM, C_BG);
    gfx_draw_string(470, y0, "LAST 8", C_DIM, C_BG);
    gfx_draw_string(590, y0, "SOURCE / SESSIONS / BEST", C_DIM, C_BG);
    for (int ch = from; ch < to; ch++) {
        unsigned int y = y0 + 22 + (unsigned int)(ch - from) * 28;
        char b[48]; unsigned int n = 0;
        int l = chan_level(ch);
        gfx_fill_rect(16, y - 3, 768, 26, C_PANEL);
        gfx_draw_string(20, y + 3, chcfg[ch].label, C_TXT, C_PANEL);
        b[0] = 0;
        if (cnt[ch] == 0) {
            put_str(b, &n, "--");
        } else {
            put_num(b, &n, chan_get(ch, 0)); put_str(b, &n, " "); put_str(b, &n, chcfg[ch].unit);
        }
        gfx_draw_string(124, y + 3, b, C_TXT, C_PANEL);
        n = 0; b[0] = 0;
        if (cnt[ch] == 0) {
            put_str(b, &n, "no data yet");
        } else if (l < 0) {
            put_str(b, &n, "Building baseline ("); put_num(b, &n, cnt[ch]);
            put_str(b, &n, "/"); put_num(b, &n, (unsigned int)chcfg[ch].base_n); put_str(b, &n, ")");
        } else {
            put_str(b, &n, level_word(l));
        }
        gfx_fill_rect(232, y + 3, 12, 12, level_col(l));
        gfx_draw_string(250, y + 3, b, level_col(l), C_PANEL);
        /* bar strip: last 8 entries, oldest on the left */
        unsigned int have = cnt[ch] < HIST ? cnt[ch] : HIST;
        unsigned int mx = 1;
        for (unsigned int k = 0; k < have; k++) { unsigned int v = chan_get(ch, k); if (v > mx) mx = v; }
        for (unsigned int k = 0; k < have; k++) {
            unsigned int v = chan_get(ch, have - 1 - k);
            unsigned int h = 3 + (v * 17) / mx;
            gfx_fill_rect(470 + k * 14, y + 20 - h, 10, h, l < 0 ? C_ACC : level_col(l));
        }
        n = 0; b[0] = 0;
        put_str(b, &n, chcfg[ch].tag); put_str(b, &n, " n="); put_num(b, &n, sess[ch]);
        if (ch >= CH_REACT && sess[ch] > 0) {
            put_str(b, &n, " best "); put_num(b, &n, tmr_read(&best_t[ch], 0));
        }
        gfx_draw_string(590, y + 3, b, C_DIM, C_PANEL);
    }
}

static void draw_attention(unsigned int y)
{
    char b[120]; int l = 0;
    unsigned int now = timer_ticks();
    if (attn_build(now, b, sizeof b, &l)) {
        gfx_draw_string(20, y, "GROUND ATTENTION:", level_col(l), C_BG);
        draw_wrapped(20, y + 18, b, 94, level_col(l), C_BG, 16);
    } else {
        gfx_draw_string(20, y, "GROUND ATTENTION: no indicator needs attention.", C_OK, C_BG);
    }
}

static void draw_menu(void)
{
    char b[60]; unsigned int n = 0;
    gfx_draw_string(20, 124, "MENU", C_ACC, C_BG);
    gfx_draw_string_scaled(40, 144, "1  Health guide", C_TXT, C_BG, 2);
    gfx_draw_string_scaled(40, 180, "2  Check-in", C_TXT, C_BG, 2);
    gfx_draw_string_scaled(40, 216, "3  Reaction test", C_TXT, C_BG, 2);
    gfx_draw_string_scaled(420, 144, "4  Memory test", C_TXT, C_BG, 2);
    gfx_draw_string_scaled(420, 180, "5  Mental math", C_TXT, C_BG, 2);
    gfx_draw_string_scaled(420, 216, "6  Enter vitals", C_TXT, C_BG, 2);
    draw_table(264, 0, NCH);
    b[0] = 0;
    put_str(b, &n, "Sessions completed this boot: ");
    put_num(b, &n, sess[CH_REACT] + sess[CH_MEM] + sess[CH_MATH]);
    gfx_draw_string(20, 430, b, C_ACC, C_BG);
    gfx_draw_string(20, 448, "Sources: vitals, sugar, mood ENTERED by you | tests MEASURED from you | demo SIMULATED", C_DIM, C_BG);
    if (demo_active) gfx_draw_string(20, 466, "DEMO HISTORY (SIMULATED) is loaded - those entries are not real measurements.", C_AMBER, C_BG);
    draw_attention(490);
    draw_footer("1-6: choose  D: demo  Esc: close");
}

static void draw_guide(void)
{
    char head[48];
    unsigned int n = 0;
    int y;

    head[0] = 0;
    put_str(head, &n, "HEALTH GUIDE  ");
    put_num(head, &n, (unsigned int)guide_page + 1);
    put_str(head, &n, "/");
    put_num(head, &n, GUIDE_PAGES);
    gfx_draw_string(20, 122, head, C_ACC, C_BG);
    gfx_draw_string_scaled(20, 140, guide_title(guide_page), C_TXT, C_BG, 2);
    gfx_draw_string(400, 148, "DATA: ", C_DIM, C_BG);
    gfx_draw_string(448, 148, guide_source(guide_page), C_AMBER, C_BG);

    y = 180;
    for (int k = 0; k < GUIDE_PARTS; k++) {
        gfx_draw_string(20, (unsigned int)y, guide_part_name(k), C_ACC, C_BG);
        y = draw_wrapped(36, (unsigned int)y + 17, guide_text(guide_page, k), 92, C_TXT, C_BG, 16);
        y += 6;
    }
    draw_footer("Left/Right or N/P: page    Esc: menu");
}

static void draw_sugar(void)
{
    char b[8]; unsigned int i;
    gfx_draw_string(20, 124, "CHECK-IN 1/2: BLOOD SUGAR  (ENTERED BY THE ASTRONAUT)", C_ACC, C_BG);
    draw_wrapped(20, 150, "Type the value you measured, in mg/dL, then press Enter. Backspace fixes a digit. Press Enter with nothing typed to skip.", 94, C_TXT, C_BG, 18);
    for (i = 0; i < ilen; i++) b[i] = ibuf[i];
    b[ilen] = '_'; b[ilen + 1] = 0;
    gfx_draw_string_scaled(40, 230, b, C_TXT, C_BG, 5);
    gfx_draw_string_scaled(300, 250, "mg/dL", C_DIM, C_BG, 2);
    draw_footer("0-9 type  Bksp fix  Enter save  Esc menu");
}

static void draw_mood(void)
{
    char b[40]; unsigned int n = 0;
    gfx_draw_string(20, 124, "CHECK-IN 2/2: MOOD  (ENTERED BY THE ASTRONAUT)", C_ACC, C_BG);
    if (sugar_saved) {
        put_str(b, &n, "Blood sugar saved: "); put_num(b, &n, sugar_saved); put_str(b, &n, " mg/dL");
        gfx_draw_string(20, 146, b, C_OK, C_BG);
    }
    draw_wrapped(20, 176, "How do you feel right now? Press a key from 1 to 5.", 94, C_TXT, C_BG, 18);
    for (int k = 1; k <= 5; k++) {
        char d[2] = { (char)('0' + k), 0 };
        gfx_fill_rect(40 + (unsigned int)(k - 1) * 140, 230, 120, 120, C_PANEL);
        gfx_draw_string_scaled(84 + (unsigned int)(k - 1) * 140, 262, d, C_TXT, C_PANEL, 6);
    }
    gfx_draw_string(40, 366, "1 = low", C_DIM, C_BG);
    gfx_draw_string(600, 366, "5 = good", C_DIM, C_BG);
    draw_footer("1-5: mood    Esc: menu");
}

static void draw_ci_done(void)
{
    gfx_draw_string(20, 124, "CHECK-IN SAVED", C_OK, C_BG);
    draw_table(160, 0, 2);
    gfx_draw_string(20, 260, "Values are compared only with your own earlier entries (demo thresholds).", C_DIM, C_BG);
    gfx_draw_string(20, 280, "Follow mission medical procedures. Report to the flight surgeon if a status stays AMBER or RED.", C_DIM, C_BG);
    draw_attention(320);
    draw_footer("Esc: menu");
}

static void draw_react(void)
{
    char b[64]; unsigned int n;
    if (st == S_R_WAIT) {
        gfx_fill_rect(0, 90, 800, 480, C_WAIT);
        draw_header();
        gfx_draw_string_scaled(200, 280, "WAIT FOR GREEN...", C_TXT, C_WAIT, 4);
        gfx_draw_string(20, 576, "Do not press yet. Esc: stop", C_DIM, C_BG);
        return;
    }
    if (st == S_R_GO) {
        gfx_fill_rect(0, 90, 800, 480, C_GRN_BG);
        draw_header();
        gfx_draw_string_scaled(180, 270, "PRESS SPACE NOW", C_TXT, C_GRN_BG, 5);
        return;
    }
    gfx_draw_string(20, 124, "REACTION TEST  (MEASURED from you)", C_ACC, C_BG);
    if (st == S_R_MSG) {
        gfx_draw_string_scaled(20, 160, "TOO EARLY", C_FAULT, C_BG, 3);
        draw_wrapped(20, 214, "That trial is repeated. Wait for the screen to turn green, then press Space. Press Space to try again.", 94, C_TXT, C_BG, 18);
    } else if (st == S_R_IDLE) {
        n = 0; b[0] = 0;
        put_str(b, &n, "Trial "); put_num(b, &n, r_trial + 1); put_str(b, &n, " of 5. Press SPACE to start.");
        gfx_draw_string_scaled(20, 156, b, C_TXT, C_BG, 2);
        draw_wrapped(20, 200, "After a random wait of 1.5 to 4 seconds the screen turns GREEN. Then press Space as fast as you can. Pressing too early repeats the trial.", 94, C_DIM, C_BG, 18);
    } else {
        gfx_draw_string_scaled(20, 156, "SESSION DONE", C_OK, C_BG, 3);
        n = 0; b[0] = 0;
        put_str(b, &n, "Result: "); put_num(b, &n, r_result); put_str(b, &n, " ms (median of 5 trials)");
        gfx_draw_string_scaled(20, 210, b, C_TXT, C_BG, 2);
    }
    for (unsigned int k = 0; k < r_trial && k < REACT_TRIALS; k++) {
        n = 0; b[0] = 0;
        put_str(b, &n, "Trial "); put_num(b, &n, k + 1); put_str(b, &n, ": "); put_num(b, &n, r_ms[k]); put_str(b, &n, " ms");
        gfx_draw_string(20, 250 + k * 18, b, C_TXT, C_BG);
    }
    gfx_draw_string(20, 346, "Timer resolution: 10 ms (PIT 100 Hz). Higher = slower. Compared only with your own baseline.", C_DIM, C_BG);
    if (st == S_R_DONE) draw_table(376, CH_REACT, CH_REACT + 1);
    if (st == S_R_DONE) draw_attention(436);
    draw_footer(st == S_R_DONE ? "Space: run again    Esc: menu" : "Space: continue    Esc: menu");
}

static void draw_pad(void)
{
    static const unsigned int dim[4] = { 0x00601818u, 0x00186018u, 0x00181860u, 0x00606018u };
    static const unsigned int lit[4] = { 0x00FF4040u, 0x0040FF40u, 0x004060FFu, 0x00FFFF40u };
    for (int k = 0; k < 4; k++) {
        char d[2] = { (char)('1' + k), 0 };
        unsigned int x = 250 + (unsigned int)(k % 2) * 160, y = 330 + (unsigned int)(k / 2) * 120;
        gfx_fill_rect(x, y, 140, 100, m_lit == k ? lit[k] : dim[k]);
        gfx_draw_string_scaled(x + 56, y + 34, d, C_TXT, m_lit == k ? lit[k] : dim[k], 4);
    }
}

static void draw_mem(void)
{
    char b[64]; unsigned int n = 0;
    gfx_draw_string(20, 124, "MEMORY TEST  (MEASURED from you)", C_ACC, C_BG);
    if (st == S_M_IDLE) {
        gfx_draw_string_scaled(20, 156, "Press SPACE to start", C_TXT, C_BG, 2);
        draw_wrapped(20, 200, "Watch the colour pads light up. Then repeat the order with keys 1-4. The sequence grows by one each round. Your result is the longest sequence you repeat correctly.", 94, C_DIM, C_BG, 18);
        draw_footer("Space: start    Esc: menu");
        return;
    }
    if (st == S_M_DONE) {
        gfx_draw_string_scaled(20, 156, "SESSION DONE", C_OK, C_BG, 3);
        put_str(b, &n, "Longest correct sequence: "); put_num(b, &n, m_longest);
        gfx_draw_string_scaled(20, 210, b, C_TXT, C_BG, 2);
        draw_table(260, CH_MEM, CH_MEM + 1);
        draw_attention(330);
        draw_footer("Space: run again    Esc: menu");
        return;
    }
    put_str(b, &n, "Sequence length "); put_num(b, &n, m_len);
    gfx_draw_string_scaled(20, 156, b, C_TXT, C_BG, 2);
    n = 0; b[0] = 0;
    if (st == S_M_SHOW) put_str(b, &n, "Watch...");
    else { put_str(b, &n, "Your turn: "); put_num(b, &n, m_pos); put_str(b, &n, " of "); put_num(b, &n, m_len); }
    gfx_draw_string_scaled(20, 200, b, st == S_M_SHOW ? C_AMBER : C_OK, C_BG, 2);
    draw_pad();
    draw_footer("1-4: repeat the order    Esc: menu");
}

static void draw_math(void)
{
    char b[64]; unsigned int n = 0;
    gfx_draw_string(20, 124, "MENTAL MATH  (MEASURED from you)", C_ACC, C_BG);
    if (st == S_Q_IDLE) {
        gfx_draw_string_scaled(20, 156, "Press SPACE to start", C_TXT, C_BG, 2);
        draw_wrapped(20, 200, "5 simple questions (+, -, x). Type each answer and press Enter. Your result is the number correct.", 94, C_DIM, C_BG, 18);
        draw_footer("Space: start    Esc: menu");
        return;
    }
    if (st == S_Q_DONE) {
        gfx_draw_string_scaled(20, 156, "SESSION DONE", C_OK, C_BG, 3);
        put_str(b, &n, "Correct: "); put_num(b, &n, q_score); put_str(b, &n, " of 5");
        gfx_draw_string_scaled(20, 210, b, C_TXT, C_BG, 2);
        draw_table(260, CH_MATH, CH_MATH + 1);
        draw_attention(330);
        draw_footer("Space: run again    Esc: menu");
        return;
    }
    put_str(b, &n, "Question "); put_num(b, &n, q_i + 1); put_str(b, &n, " of 5");
    gfx_draw_string(20, 150, b, C_DIM, C_BG);
    n = 0; b[0] = 0;
    put_num(b, &n, q_a); put_str(b, &n, " "); b[n++] = q_op; b[n] = 0; put_str(b, &n, " "); put_num(b, &n, q_b); put_str(b, &n, " = ?");
    gfx_draw_string_scaled(40, 190, b, C_TXT, C_BG, 5);
    n = 0; b[0] = 0;
    for (unsigned int k = 0; k < ilen; k++) b[n++] = ibuf[k];
    b[n++] = '_'; b[n] = 0;
    gfx_draw_string_scaled(40, 290, b, C_ACC, C_BG, 5);
    if (q_last == 1) gfx_draw_string(40, 400, "Previous answer: correct", C_OK, C_BG);
    else if (q_last == 2) {
        n = 0; b[0] = 0;
        put_str(b, &n, "Previous answer: wrong (it was "); put_num(b, &n, q_last_ans); put_str(b, &n, ")");
        gfx_draw_string(40, 400, b, C_FAULT, C_BG);
    }
    draw_footer("0-9 type  Bksp fix  Enter answer  Esc menu");
}

/* ---- vitals typed in by the astronaut (same path as the serial lines: sensor_feed) ---- */
#define VITAL_LOCK_N 10      /* vigil.c LOCK_AFTER: readings before a vital's baseline locks */
typedef struct { const char *name, *unit, *wire; char kind; int tenths; } vitcfg_t;
static const vitcfg_t vitcfg[5] = {
    { "Heart rate",               "bpm",  "hr",    'h', 0 },
    { "SpO2",                     "%",    "spo2",  'o', 0 },
    { "Blood pressure (systolic)", "mmHg", "bp",   'p', 0 },
    { "Body temperature",         "C",    "temp",  't', 1 },
    { "Sleep last night",         "hours","sleep", 's', 1 },
};

static void fmt_vit(char *b, unsigned int *n, int i, unsigned int v)
{
    if (vitcfg[i].tenths) { put_num(b, n, v / 10); put_str(b, n, "."); put_num(b, n, v % 10); }
    else put_num(b, n, v);
}

static void draw_vit(void)
{
    char b[80]; unsigned int n;
    gfx_draw_string(20, 124, "ENTER VITALS  (ENTERED BY THE ASTRONAUT)", C_ACC, C_BG);
    if (st == S_VIT) {
        n = 0; b[0] = 0;
        put_str(b, &n, "Vital "); put_num(b, &n, vit_idx + 1); put_str(b, &n, " of 5: "); put_str(b, &n, vitcfg[vit_idx].name);
        gfx_draw_string_scaled(20, 148, b, C_TXT, C_BG, 2);
        n = 0; b[0] = 0;
        for (unsigned int k = 0; k < ilen; k++) b[n++] = ibuf[k];
        b[n++] = '_'; b[n] = 0;
        gfx_draw_string_scaled(40, 190, b, C_TXT, C_BG, 4);
        n = 0; b[0] = 0;
        if (vitcfg[vit_idx].tenths) {
            unsigned int v = 0;
            for (unsigned int k = 0; k < ilen; k++) v = v * 10 + (unsigned int)(ibuf[k] - '0');
            if (ilen > 0) { fmt_vit(b, &n, (int)vit_idx, v); put_str(b, &n, " "); put_str(b, &n, vitcfg[vit_idx].unit); }
            else put_str(b, &n, vit_idx == 3 ? "type tenths: 366 means 36.6 C" : "type tenths: 75 means 7.5 hours");
        } else {
            put_str(b, &n, vitcfg[vit_idx].unit);
        }
        gfx_draw_string(300, 214, b, C_DIM, C_BG);
        gfx_draw_string(20, 262, "Type the value you measured, then press Enter. Enter with nothing typed skips this vital.", C_DIM, C_BG);
    } else {
        gfx_draw_string_scaled(20, 148, "ENTRIES SAVED", C_OK, C_BG, 2);
        gfx_draw_string(20, 190, "Press Space to enter another set, Esc for the menu.", C_DIM, C_BG);
    }
    gfx_draw_string(20, 292, "VITAL", C_DIM, C_BG);
    gfx_draw_string(250, 292, "LAST ENTERED", C_DIM, C_BG);
    gfx_draw_string(400, 292, "STATUS vs YOUR OWN BASELINE", C_DIM, C_BG);
    for (int i = 0; i < 5; i++) {
        unsigned int y = 314 + (unsigned int)i * 28;
        int locked = vigil_vital_locked(vitcfg[i].kind);
        level_t l = vigil_vital_level(vitcfg[i].kind);
        unsigned int col = !locked ? C_DIM : l == LVL_RED ? C_FAULT : l == LVL_AMBER ? C_AMBER : C_OK;
        gfx_fill_rect(16, y - 3, 768, 26, C_PANEL);
        gfx_draw_string(20, y + 3, vitcfg[i].name, C_TXT, C_PANEL);
        n = 0; b[0] = 0;
        if (vit_val[i]) { fmt_vit(b, &n, i, vit_val[i]); put_str(b, &n, " "); put_str(b, &n, vitcfg[i].unit); }
        else put_str(b, &n, "--");
        gfx_draw_string(250, y + 3, b, C_TXT, C_PANEL);
        n = 0; b[0] = 0;
        if (!locked) {
            put_str(b, &n, "Learning baseline: entries this boot "); put_num(b, &n, vit_cnt[i]);
            put_str(b, &n, " (locks after "); put_num(b, &n, VITAL_LOCK_N); put_str(b, &n, ")");
        } else put_str(b, &n, l == LVL_RED ? "RED" : l == LVL_AMBER ? "AMBER" : "GREEN");
        gfx_fill_rect(400, y + 3, 12, 12, col);
        gfx_draw_string(418, y + 3, b, col, C_PANEL);
    }
    gfx_draw_string(20, 470, "Compared only with your own baseline (demo thresholds). Follow mission medical procedures;", C_DIM, C_BG);
    gfx_draw_string(20, 488, "report to the flight surgeon if a status stays AMBER or RED.", C_DIM, C_BG);
    draw_footer("0-9 type  Bksp fix  Enter save  Esc menu");
}

static void draw_all(void)
{
    if (st == S_R_WAIT || st == S_R_GO) { draw_react(); return; }
    gfx_fill_rect(0, 90, 800, 480, C_BG);
    draw_header();
    draw_strip();
    switch (st) {
    case S_MENU:    draw_menu();    break;
    case S_GUIDE:   draw_guide();   break;
    case S_SUGAR:   draw_sugar();   break;
    case S_MOOD:    draw_mood();    break;
    case S_CI_DONE: draw_ci_done(); break;
    case S_R_IDLE: case S_R_MSG: case S_R_DONE: draw_react(); break;
    case S_M_IDLE: case S_M_SHOW: case S_M_INPUT: case S_M_DONE: draw_mem(); break;
    case S_Q_IDLE: case S_Q: case S_Q_DONE: draw_math(); break;
    case S_VIT: case S_VIT_DONE: draw_vit(); break;
    default: break;
    }
}

/* ---- public API ---- */
int astro_is_open(void) { return st != S_CLOSED; }

void astro_open(void)
{
    st = S_MENU;
    dirty = 1;
    last_strip = timer_ticks();
}

/* Key D in the menu: deterministic SIMULATED history so drift to AMBER/RED can be shown quickly */
static void load_demo(void)
{
    static const unsigned char sugar[8] = { 100, 98, 102, 100, 101, 125, 128, 130 };   /* ~+28%: RED  */
    static const unsigned char mood[8]  = { 4, 4, 4, 4, 4, 3, 3, 3 };                  /* -25%: RED   */
    static const unsigned short react[6] = { 300, 310, 305, 340, 350, 345 };           /* ~+13%: AMBER */
    static const unsigned char mem[6]   = { 6, 6, 6, 5, 5, 5 };                        /* -17%: AMBER */
    static const unsigned char mth[6]   = { 5, 5, 5, 4, 4, 4 };                        /* -20%: AMBER */
    for (int ch = 0; ch < NCH; ch++) {
        for (int i = 0; i < HIST; i++) tmr_write(&ring[ch][i], 0);
        tmr_write(&base_t[ch], 0); tmr_write(&best_t[ch], 0);
        cnt[ch] = 0; sess[ch] = 0; base_set[ch] = 0;
    }
    ser_str("DEMO,start,sim=1\n");
    loading = 1;
    for (int i = 0; i < 8; i++) chan_add(CH_SUGAR, sugar[i]);
    for (int i = 0; i < 8; i++) chan_add(CH_MOOD, mood[i]);
    for (int i = 0; i < 6; i++) chan_add(CH_REACT, react[i]);
    for (int i = 0; i < 6; i++) chan_add(CH_MEM, mem[i]);
    for (int i = 0; i < 6; i++) chan_add(CH_MATH, mth[i]);
    loading = 0;
    for (int ch = 0; ch < NCH; ch++) sess[ch] = 0;      /* demo entries are not "sessions this boot" */
    demo_active = 1;
    main_dirty = 1;
}

int astro_demo_active(void) { return demo_active; }
int astro_vit_sim(void) { return vit_sim; }
void astro_set_vit_sim(int v) { vit_sim = v; }

static void to_menu(void) { st = S_MENU; dirty = 1; }

static void start_wait(unsigned int now)
{
    r_wait_until = now + 150 + (rnd() % 251);      /* 1.5 .. 4.0 s */
    st = S_R_WAIT;
    dirty = 1;
}

static int median5(unsigned int *a)
{
    sort_u(a, REACT_TRIALS);
    return (int)a[REACT_TRIALS / 2];
}

static void mem_start_show(unsigned int now)
{
    st = S_M_SHOW; m_show_i = 0; m_lit = -1; m_until = now + 60; dirty = 1;
}

static void q_next(void)
{
    q_a = 0; q_b = 0;
    unsigned int o = rnd() % 3;
    if (o == 0)      { q_op = '+'; q_a = 1 + rnd() % 20; q_b = 1 + rnd() % 20; }
    else if (o == 1) { q_op = '-'; q_a = 1 + rnd() % 20; q_b = 1 + rnd() % q_a; }
    else             { q_op = 'x'; q_a = 2 + rnd() % 8;  q_b = 2 + rnd() % 8; }
    ilen = 0;
}

void astro_key(char c)
{
    unsigned int now = timer_ticks();      /* time of this key event (10 ms resolution) */
    if (st == S_CLOSED) return;

    if (st == S_R_GO) {
        if (c == ' ') {
            r_ms[r_trial++] = (now - r_go_tick) * 10u;
            if (r_trial >= REACT_TRIALS) {
                unsigned int t[REACT_TRIALS];
                for (int k = 0; k < REACT_TRIALS; k++) t[k] = r_ms[k];
                r_result = (unsigned int)median5(t);
                chan_add(CH_REACT, r_result);
                st = S_R_DONE;
            } else {
                st = S_R_IDLE;
            }
            dirty = 1;
        } else if (c == 27) to_menu();
        return;
    }
    if (st == S_R_WAIT) {
        if (c == ' ') { r_early = 1; st = S_R_MSG; dirty = 1; }   /* pressed early: repeat this trial */
        else if (c == 27) to_menu();
        return;
    }

    if (c == 27) {
        if (st == S_MENU) st = S_CLOSED;
        else if (st == S_MOOD && sugar_saved) { to_menu(); }
        else to_menu();
        return;
    }

    switch (st) {
    case S_MENU:
        if (c == '1') { st = S_GUIDE; guide_page = 0; dirty = 1; }
        else if (c == '2') { st = S_SUGAR; ilen = 0; sugar_saved = 0; dirty = 1; }
        else if (c == '3') { st = S_R_IDLE; r_trial = 0; rng ^= now; dirty = 1; }
        else if (c == '4') { st = S_M_IDLE; rng ^= now; dirty = 1; }
        else if (c == '5') { st = S_Q_IDLE; rng ^= now; dirty = 1; }
        else if (c == 'd') { load_demo(); dirty = 1; }
        else if (c == '6') { st = S_VIT; vit_idx = 0; ilen = 0; dirty = 1; }
        break;
    case S_GUIDE:
        if (c == 0x12 || c == 'n') { guide_page = (guide_page + 1) % GUIDE_PAGES; dirty = 1; }
        else if (c == 0x11 || c == 'p') { guide_page = (guide_page + GUIDE_PAGES - 1) % GUIDE_PAGES; dirty = 1; }
        break;
    case S_SUGAR:
        if (c >= '0' && c <= '9' && ilen < 3) { ibuf[ilen++] = c; dirty = 1; }
        else if (c == '\b' && ilen > 0) { ilen--; dirty = 1; }
        else if (c == '\n') {
            unsigned int v = 0;
            for (unsigned int k = 0; k < ilen; k++) v = v * 10 + (unsigned int)(ibuf[k] - '0');
            if (ilen > 0 && v > 0) {
                chan_add(CH_SUGAR, v);
                sugar_saved = v;
                last_checkin = now;
                st = S_MOOD; dirty = 1;
            } else if (ilen == 0) {
                sugar_saved = 0; st = S_MOOD; dirty = 1;    /* skip */
            }
        }
        break;
    case S_MOOD:
        if (c >= '1' && c <= '5') {
            chan_add(CH_MOOD, (unsigned int)(c - '0'));
            last_checkin = now;
            st = S_CI_DONE; dirty = 1;
        }
        break;
    case S_R_IDLE:
    case S_R_MSG:
        if (c == ' ') start_wait(now);
        break;
    case S_R_DONE:
        if (c == ' ') { st = S_R_IDLE; r_trial = 0; dirty = 1; }
        break;
    case S_M_IDLE:
    case S_M_DONE:
        if (c == ' ') {
            for (int k = 0; k < MEM_MAX; k++) m_seq[k] = (unsigned char)(rnd() % 4);
            m_len = 1; m_longest = 0;
            mem_start_show(now);
        }
        break;
    case S_M_INPUT:
        if (c >= '1' && c <= '4') {
            if ((unsigned int)(c - '1') == m_seq[m_pos]) {
                m_pos++;
                if (m_pos >= m_len) {
                    m_longest = m_len;
                    if (m_len >= MEM_MAX) { chan_add(CH_MEM, m_longest); st = S_M_DONE; dirty = 1; }
                    else { m_len++; mem_start_show(now); }
                } else dirty = 1;
            } else {                                          /* wrong: session over */
                chan_add(CH_MEM, m_longest);
                st = S_M_DONE; dirty = 1;
            }
        }
        break;
    case S_VIT:
        if (c >= '0' && c <= '9' && ilen < 3) { ibuf[ilen++] = c; dirty = 1; }
        else if (c == '\b' && ilen > 0) { ilen--; dirty = 1; }
        else if (c == '\n') {
            unsigned int v = 0;
            for (unsigned int k = 0; k < ilen; k++) v = v * 10 + (unsigned int)(ibuf[k] - '0');
            if (ilen > 0 && v == 0) break;                     /* 0 is not a reading: type again or skip */
            if (ilen > 0) {
                /* same path as a serial line: 'h072\n' */
                sensor_feed(vitcfg[vit_idx].kind);
                sensor_feed((char)('0' + v / 100)); sensor_feed((char)('0' + (v / 10) % 10)); sensor_feed((char)('0' + v % 10));
                sensor_feed('\n');
                vit_val[vit_idx] = v; vit_cnt[vit_idx]++;
                vit_sim = 0;
                ser_str("VIT "); ser_str(vitcfg[vit_idx].wire); ser_str(" "); ser_num(v); ser_str("\n");
            }
            ilen = 0; vit_idx++;
            if (vit_idx >= 5) st = S_VIT_DONE;
            dirty = 1;
        }
        break;
    case S_VIT_DONE:
        if (c == ' ') { st = S_VIT; vit_idx = 0; ilen = 0; dirty = 1; }
        break;
    case S_Q_IDLE:
    case S_Q_DONE:
        if (c == ' ') { st = S_Q; q_i = 0; q_score = 0; q_last = 0; q_next(); dirty = 1; }
        break;
    case S_Q:
        if (c >= '0' && c <= '9' && ilen < 3) { ibuf[ilen++] = c; dirty = 1; }
        else if (c == '\b' && ilen > 0) { ilen--; dirty = 1; }
        else if (c == '\n' && ilen > 0) {
            unsigned int v = 0;
            for (unsigned int k = 0; k < ilen; k++) v = v * 10 + (unsigned int)(ibuf[k] - '0');
            unsigned int ans = q_op == '+' ? q_a + q_b : q_op == '-' ? q_a - q_b : q_a * q_b;
            if (v == ans) { q_score++; q_last = 1; } else { q_last = 2; q_last_ans = ans; }
            q_i++;
            if (q_i >= MATH_QUESTIONS) { chan_add(CH_MATH, q_score); st = S_Q_DONE; }
            else q_next();
            dirty = 1;
        }
        break;
    default: break;
    }
}

void astro_tick(unsigned int now)
{
    /* main-screen bookkeeping runs even when the console is closed */
    int due = astro_checkin_due(now);
    if (due != prev_due) { prev_due = due; main_dirty = 1; }
    if ((now / 300) != prev_rot) { prev_rot = now / 300; main_dirty = 1; }

    if (st == S_CLOSED) return;
    if (st == S_R_WAIT && now >= r_wait_until) {      /* screen turns green */
        r_go_tick = now;
        st = S_R_GO;
        dirty = 1;
    }
    if (st == S_M_SHOW && now >= m_until) {           /* memory playback: lit 0.5 s, gap 0.2 s */
        if (m_lit < 0) {
            if (m_show_i < m_len) { m_lit = m_seq[m_show_i]; m_until = now + 50; draw_pad(); }
            else { st = S_M_INPUT; m_pos = 0; dirty = 1; }
        } else {
            m_lit = -1; m_show_i++; m_until = now + 20; draw_pad();
        }
    }
    if (now - last_strip >= 100) {            /* keep the simulated-vitals strip fresh */
        last_strip += 100;
        if (st != S_R_WAIT && st != S_R_GO) draw_strip();
    }
}

void astro_draw(void)
{
    if (st == S_CLOSED || !dirty) return;
    dirty = 0;
    draw_all();
}

int astro_checkin_due(unsigned int now) { return now - last_checkin >= CHECKIN_INTERVAL_TICKS; }

int astro_attention(unsigned int now, char *out, unsigned int cap, int *level)
{
    return attn_build(now, out, cap, level);
}

int astro_take_dirty(void) { int d = main_dirty; main_dirty = 0; return d; }

/* ---- TMR: fault injection and scrubbing of the astronaut channels ---- */
static const char *chname(int ch) { return chcfg[ch].name; }

int astro_inject_fault(void)
{
    for (int tries = 0; tries < NCH; tries++) {
        int ch = (inj_next + tries) % NCH;
        if (cnt[ch] == 0) continue;
        tmr_u32 *t = (base_set[ch] && (a_inj & 1)) ? &base_t[ch] : &ring[ch][(cnt[ch] - 1) % HIST];
        tmr_inject(t, inj_copy, 0x80);
        ser_str("AINJ,tick="); ser_num(timer_ticks());
        ser_str(",chan="); ser_str(chname(ch));
        ser_str(",copy="); { char cc[2] = { (char)('A' + inj_copy), 0 }; ser_str(cc); }
        ser_str("\n");
        inj_copy = (inj_copy + 1) % 3;
        inj_next = (ch + 1) % NCH;
        a_inj++;
        main_dirty = 1;
        return 1;
    }
    return 0;
}

int astro_scrub(unsigned int now)
{
    int repaired = 0;
    for (int ch = 0; ch < NCH; ch++) {
        for (int i = 0; i < HIST + 2; i++) {
            tmr_u32 *t = (i < HIST) ? &ring[ch][i] : (i == HIST ? &base_t[ch] : &best_t[ch]);
            tmr_status s;
            uint32_t good = tmr_read(t, &s);
            if (s != TMR_OUTVOTED) continue;
            for (int k = 0; k < 3; k++) {
                if (t->c[k] != good) {
                    ser_str("AEVT,tick="); ser_num(now);
                    ser_str(",chan="); ser_str(chname(ch));
                    ser_str(",copy="); { char cc[2] = { (char)('A' + k), 0 }; ser_str(cc); }
                    ser_str(",bad="); ser_num(t->c[k]);
                    ser_str(",good="); ser_num(good);
                    ser_str(",repaired=1\n");
                }
            }
            tmr_scrub(t);
            repaired++; a_fix++;
        }
    }
    if (repaired) dirty = dirty || (st == S_MENU || st == S_CI_DONE || st == S_R_DONE);
    return repaired;
}
