
#include "io.h"
#include "vigil.h"
#include "replay_data.h"
#include "astro.h"

void fb_write_cell(unsigned int i, char c, unsigned char fg, unsigned char bg);
void fb_clear(void);
int fb_write(char *buf, unsigned int len);
void serial_init(unsigned short com);
int serial_write(unsigned short com, char *buf, unsigned int len);
unsigned int strlen(const char *s);
void gdt_init(void);
void idt_init(void);
void pic_remap(void);
void pit_init(unsigned int hz);
void speaker_on(unsigned int freq);
void serial_hex32(unsigned int v);
void gfx_init(unsigned int *mbi);
void gfx_fill_rect(unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned int color);
void gfx_draw_string(unsigned int x, unsigned int y, const char *s, unsigned int fg, unsigned int bg);
void gfx_draw_string_scaled(unsigned int x, unsigned int y, const char *s,
                            unsigned int fg, unsigned int bg, unsigned int scale);
int kb_pop(void);
void sensor_feed(char c);
extern unsigned int sensor_hr, sensor_spo2, sensor_temp, sensor_bp, sensor_sleep;
void gfx_putc(char c);
unsigned int timer_ticks(void);

/* ---- serial helpers (no printf) ---- */
static void ser_str(const char *s)
{
    serial_write(0x3F8, (char *)s, strlen(s));
}

static void ser_num(uint32_t v)
{
    char b[11];
    int i = 10;
    b[i] = 0;
    do { b[--i] = '0' + (v % 10); v /= 10; } while (v);
    ser_str(&b[i]);
}

static unsigned int inj_count = 0;               /* faults injected this boot */
static unsigned int fix_count = 0;               /* repairs the scrubber made */

static void do_fault(void)                       /* one fault, logged */
{
    if (vigil_inject_fault()) {
        inj_count++;
        ser_str("INJ,tick="); ser_num(timer_ticks()); ser_str("\n");
    }
}

/* ---- serial input: h072 / o098 / t366 / b1 (fault trigger) ---- */
static char sline[8];
static int  sn = 0;

static void serial_poll(void)
{
    while (inb(0x3F8 + 5) & 1) {                 /* UART has a byte waiting */
        char c = (char)inb(0x3F8);
        if (c == '\r') continue;
        if (c != '\n') {
            if (sn < 7) sline[sn++] = c;
            continue;
        }
        if (sn == 2 && sline[0] == 'b' && sline[1] == '1') {
            do_fault();                          /* fault trigger */
        } else if (sn == 4) {                    /* a vitals line: sensor_feed checks it */
            for (int i = 0; i < 4; i++) sensor_feed(sline[i]);
            sensor_feed('\n');
        }                                        /* w1 / w0 and anything else: ignored */
        sn = 0;
    }
}

static void fmt3(char *out, uint32_t v)         /* 72 -> "072", over 999 -> "???" */
{
    if (v > 999) { out[0] = out[1] = out[2] = '?'; out[3] = 0; return; }
    out[0] = '0' + (v / 100);
    out[1] = '0' + ((v / 10) % 10);
    out[2] = '0' + (v % 10);
    out[3] = 0;
}

/* ================= screen (800 x 600) =================
 * Layout:  header 0-90 | baseline + status 104-300 |
 *          fault + replay 312-470 | next step 478-560 | footer 576
 * Static parts are drawn once (draw_frame). The draw_* functions below
 * only repaint the inside of their own panel.
 */
#define C_BG     0x00101820u
#define C_HDR    0x000A121Bu
#define C_PANEL  0x00162231u
#define C_BOX    0x000B141Eu
#define C_LINE   0x002C4560u
#define C_DIM    0x0080A0C0u
#define C_TXT    0x00FFFFFFu
#define C_ACC    0x0040C8FFu
#define C_GREEN  0x0000C000u
#define C_AMBER  0x00FFBF00u
#define C_RED    0x00C00000u
#define C_FAULT  0x00FF4040u
#define C_OK     0x0000FF00u

/* replay state lives at file level so the drawing code can show it */
static int replay_on   = 0;                      /* 1 while the replay runs */
static int replay_day  = 0;                      /* day just fed, 0..55 */
static int replay_done = 0;                      /* 1 after a full replay */

static unsigned int text_w(const char *s, unsigned int scale)
{
    return strlen(s) * 8u * scale;
}

static void draw_centered(unsigned int x, unsigned int w, unsigned int y,
                          const char *s, unsigned int fg, unsigned int bg,
                          unsigned int scale)
{
    unsigned int tw = text_w(s, scale);
    unsigned int off = (tw < w) ? (w - tw) / 2 : 0;
    gfx_draw_string_scaled(x + off, y, s, fg, bg, scale);
}

static void panel(unsigned int x, unsigned int y, unsigned int w, unsigned int h,
                  const char *title)
{
    gfx_fill_rect(x, y, w, h, C_LINE);                       /* border */
    gfx_fill_rect(x + 2, y + 2, w - 4, h - 4, C_PANEL);      /* inside */
    gfx_draw_string(x + 14, y + 8, title, C_ACC, C_PANEL);
}

/* ---- parts drawn once ---- */
static void draw_frame(void)
{
    gfx_fill_rect(0, 0, 800, 600, C_BG);

    /* header band */
    gfx_fill_rect(0, 0, 800, 88, C_HDR);
    gfx_fill_rect(0, 88, 800, 2, C_ACC);
    gfx_draw_string_scaled(100, 16, "A1-37 VIGIL", C_TXT, C_HDR, 3);
    gfx_draw_string(100, 66, "CREW HEALTH MONITOR  |  FAULT-TOLERANT KERNEL", C_DIM, C_HDR);
    gfx_draw_string(8, 70, "HEARTBEAT", C_DIM, C_HDR);

    /* permanent honesty badge */
    gfx_fill_rect(508, 20, 280, 48, C_AMBER);
    gfx_draw_string_scaled(512, 28, "VITALS: SIMULATED", C_BG, C_AMBER, 2);

    /* panels */
    panel(20,  104, 388, 196, "PROTECTED BASELINE - HEART RATE");
    panel(420, 104, 360, 196, "STATUS");
    panel(20,  312, 388, 158, "FAULT MONITOR");
    panel(420, 312, 360, 158, "REPLAY");
    panel(20,  478, 760,  82, "NEXT STEP");

    /* footer */
    gfx_draw_string(20,  576, "TAB: fault    R: replay    X: 20 faults    F: freeze    M: astronaut", C_DIM, C_BG);
    gfx_draw_string(620, 576, "ringzero-kernel i686", C_DIM, C_BG);
}

/* ---- parts that change ---- */
static void draw_level(void)
{
    level_t lvl = vigil_level();
    unsigned int color;
    const char *name;

    switch (lvl) {
    case LVL_GREEN: color = C_GREEN; name = "GREEN"; break;
    case LVL_AMBER: color = C_AMBER; name = "AMBER"; break;
    case LVL_RED:   color = C_RED;   name = "RED";   break;
    default:        color = C_GREEN; name = "GREEN"; break;
    }
    gfx_fill_rect(422, 130, 356, 168, C_PANEL);
    gfx_fill_rect(436, 134, 328, 152, color);
    draw_centered(436, 328, 162, name, (lvl == LVL_AMBER) ? C_BG : C_TXT, color, 6);
}

static void draw_baseline(void)
{
    char num[4];
    char lab[7];
    char ag[10];
    uint32_t vote = vigil_hr_voted();
    int agree = 0;

    gfx_fill_rect(22, 130, 384, 168, C_PANEL);

    for (int i = 0; i < 3; i++) {
        uint32_t cv = vigil_hr_copy(i);
        int bad = (cv != vote);
        unsigned int bx = 36 + i * 124;
        unsigned int by = 134;

        if (!bad) agree++;
        lab[0] = 'C'; lab[1] = 'O'; lab[2] = 'P'; lab[3] = 'Y'; lab[4] = ' ';
        lab[5] = 'A' + i; lab[6] = 0;
        if (vote == 0) { num[0] = num[1] = num[2] = '-'; num[3] = 0; }  /* not locked yet */
        else           fmt3(num, cv);

        gfx_fill_rect(bx - 2, by - 2, 114, 84, bad ? C_FAULT : C_LINE);
        gfx_fill_rect(bx, by, 110, 80, C_BOX);
        gfx_draw_string(bx + 8, by + 6, lab, bad ? C_FAULT : C_DIM, C_BOX);
        gfx_draw_string_scaled(bx + 19, by + 30, num, bad ? C_FAULT : C_TXT, C_BOX, 3);
    }

    gfx_draw_string(36, 230, "MAJORITY VOTE", C_DIM, C_PANEL);
    if (vote == 0) {
        gfx_draw_string_scaled(36, 256, "LEARNING BASELINE", C_DIM, C_PANEL, 2);
    } else {
        fmt3(num, vote);
        gfx_draw_string_scaled(36, 248, num, C_TXT, C_PANEL, 3);
        ag[0] = 'A'; ag[1] = 'G'; ag[2] = 'R'; ag[3] = 'E'; ag[4] = 'E'; ag[5] = ' ';
        ag[6] = '0' + agree; ag[7] = '/'; ag[8] = '3'; ag[9] = 0;
        gfx_draw_string_scaled(200, 262, ag, (agree == 3) ? C_OK : C_FAULT, C_PANEL, 2);
    }
}

static void draw_counts(void)                    /* kernel-computed totals */
{
    char a[4], b[4];
    fmt3(a, inj_count > 999 ? 999 : inj_count);
    fmt3(b, fix_count > 999 ? 999 : fix_count);
    gfx_draw_string(34,  448, "INJECTED", C_DIM, C_PANEL);
    gfx_draw_string(106, 448, a,          C_TXT, C_PANEL);
    gfx_draw_string(150, 448, "FIXED",    C_DIM, C_PANEL);
    gfx_draw_string(198, 448, b,          C_OK,  C_PANEL);
}

static void draw_event(void)
{
    if (astro_is_open()) return;                 /* console owns the screen */
    int copy;
    uint32_t bad, good;
    char num[4];
    char one[2];

    gfx_fill_rect(22, 338, 384, 130, C_PANEL);

    if (!vigil_event_active(timer_ticks(), &copy, &bad, &good)) {
        gfx_draw_string_scaled(34, 350, "NO FAULT ACTIVE", C_DIM, C_PANEL, 2);
        gfx_draw_string(34, 392, "Tab = inject one simulated upset", C_DIM, C_PANEL);
        draw_counts();
        return;
    }

    one[0] = 'A' + copy; one[1] = 0;
    gfx_draw_string_scaled(34,  346, "FAULT: COPY ", C_FAULT, C_PANEL, 2);
    gfx_draw_string_scaled(226, 346, one,            C_FAULT, C_PANEL, 2);
    gfx_draw_string_scaled(242, 346, " = ",           C_FAULT, C_PANEL, 2);
    fmt3(num, bad);
    gfx_draw_string_scaled(290, 346, num,            C_FAULT, C_PANEL, 2);

    gfx_draw_string_scaled(34,  382, "OUTVOTED -> ", C_TXT, C_PANEL, 2);
    fmt3(num, good);
    gfx_draw_string_scaled(226, 382, num,            C_TXT, C_PANEL, 2);

    gfx_draw_string_scaled(34,  418, "REPAIRED",     C_OK, C_PANEL, 2);
    draw_counts();
}

/* one vital: dim label + value coloured by its level ("---" until its baseline locks) */
static void vit(unsigned int x, unsigned int y, const char *label, char kind, unsigned int v, int tenths)
{
    char s[8];
    uint32_t col = C_DIM;
    level_t l = vigil_vital_level(kind);
    int n = 0;

    if (!vigil_vital_locked(kind)) {
        s[0] = s[1] = s[2] = '-'; s[3] = 0;
    } else {
        col = (l == LVL_RED) ? C_FAULT : (l == LVL_AMBER) ? C_AMBER : C_OK;
        if (tenths) {                                   /* 366 -> "36.6" */
            if (v > 999) v = 999;
            if (v >= 100) s[n++] = '0' + (v / 100);
            s[n++] = '0' + ((v / 10) % 10);
            s[n++] = '.';
            s[n++] = '0' + (v % 10);
            s[n] = 0;
        } else {
            fmt3(s, v);
        }
    }
    gfx_draw_string(x, y, label, C_DIM, C_PANEL);
    gfx_draw_string(x + 8u * 6u, y, s, col, C_PANEL);
}

static void draw_vitals(void)                           /* five vitals, only while idle */
{
    if (astro_is_open()) return;                 /* console owns the screen */
    gfx_fill_rect(422, 414, 356, 52, C_PANEL);
    vit(434, 418, "HR    ", 'h', sensor_hr,    0);
    vit(546, 418, "SPO2  ", 'o', sensor_spo2,  0);
    vit(658, 418, "TEMP  ", 't', sensor_temp,  1);
    vit(434, 440, "BP    ", 'p', sensor_bp,    0);
    vit(546, 440, "SLEEP ", 's', sensor_sleep, 1);
}

static void draw_replay(void)
{
    if (astro_is_open()) return;                 /* console owns the screen */
    gfx_fill_rect(422, 338, 356, 130, C_PANEL);

    if (!replay_on && !replay_done) {
        gfx_draw_string_scaled(434, 346, "PRESS R TO RUN", C_DIM, C_PANEL, 2);
        gfx_draw_string(434, 384, "8 weeks of simulated daily heart rate", C_DIM, C_PANEL);
        draw_vitals();
        return;
    }

    gfx_draw_string_scaled(434, 346, "SIMULATED REPLAY", C_AMBER, C_PANEL, 2);
    if (replay_on) {
        char line[9];
        line[0] = 'W'; line[1] = 'E'; line[2] = 'E'; line[3] = 'K'; line[4] = ' ';
        line[5] = '0' + (replay_day / 7 + 1); line[6] = '/'; line[7] = '8'; line[8] = 0;
        gfx_draw_string_scaled(434, 382, line, C_TXT, C_PANEL, 2);
    } else {
        gfx_draw_string_scaled(434, 382, "COMPLETE: 8 WEEKS", C_OK, C_PANEL, 2);
    }

    unsigned int pw = replay_on ? (328u * (unsigned int)(replay_day + 1)) / 56u : 328u;
    gfx_fill_rect(434, 430, 332, 16, C_LINE);
    gfx_fill_rect(436, 432, 328, 12, C_BOX);
    gfx_fill_rect(436, 432, pw, 12, C_AMBER);
}

static void draw_guidance(void)
{
    level_t l = vigil_level();

    gfx_fill_rect(22, 504, 756, 54, C_PANEL);
    if (l == LVL_AMBER) {
        gfx_draw_string_scaled(34, 514, "Rest, hydrate, re-measure within 24 hours", C_AMBER, C_PANEL, 2);
    } else if (l == LVL_RED) {
        gfx_draw_string_scaled(34, 514, "Contact the flight surgeon", C_FAULT, C_PANEL, 2);
    } else {
        gfx_draw_string_scaled(34, 514, "Continue routine monitoring", C_OK, C_PANEL, 2);
    }
}

static void draw_status(void)
{
    if (astro_is_open()) return;                 /* console owns the screen */
    draw_level();
    draw_baseline();
    draw_event();
    draw_replay();
    draw_guidance();
}

/* ---- main ---- */
int kmain(unsigned int *mbi)
{
    unsigned int last_scrub = 0, last_blink = 0, fault_last = 0, replay_last = 0;
    int banner_on = 0, blink_on = 0, fault_left = 0, console_was_open = 0;

    serial_init(0x3F8);
    gfx_init(mbi);
    draw_frame();

    gdt_init();
    idt_init();
    pic_remap();
    pit_init(100);
    outb(0x21, 0xFC);                    /* enable timer IRQ0 and keyboard IRQ1 */
    __asm__ volatile ("sti");

    draw_status();

    for (;;) {
        int c;
        while ((c = kb_pop()) != -1) {
            if (c == '\t') {
                do_fault();
            } else if (c == 'g') {                        /* DEBUG: instant baseline lock */
                for (int n = 0; n < 10; n++) {
                    sensor_feed('h'); sensor_feed('0'); sensor_feed('7');
                    sensor_feed('2'); sensor_feed('\n');
                }
            } else if (c == 'r') {                        /* 8-week SIMULATED replay */
                vigil_reset();
                ser_str("REPLAY,start,sim=1\n");
                replay_on = 1; replay_day = 0; replay_done = 0; replay_last = timer_ticks();
                draw_status();
            } else if (c == 'x') {                        /* 20 automatic faults for the evidence table */
                ser_str("RUN,faults=20\n");
                fault_left = 20;
                fault_last = timer_ticks() - 150;
            } else if (c == 'f') {                        /* FREEZE: stop everything, square stops blinking */
                ser_str("FREEZE\n");
                __asm__ volatile ("cli");
                for (;;) __asm__ volatile ("hlt");
            } else if (c == 'm' && !astro_is_open()) {         /* open the astronaut console */
                astro_open();
            } else if (astro_is_open()) {                     /* console owns the keyboard */
                astro_key((char)c);
            } else if (c == 27 || c == 0x11 || c == 0x12) {   /* Esc / arrows: ignored here */
            } else {
                gfx_putc((char)c);
                sensor_feed((char)c);
            }
        }

        if (console_was_open && !astro_is_open()) {        /* console just closed: restore main screen */
            draw_frame();
            draw_status();
        }
        console_was_open = astro_is_open();
        astro_tick(timer_ticks());
        astro_draw();

        serial_poll();

        /* 20-fault run: one fault every 1.5 s, so each is repaired before the next */
        if (fault_left > 0 && timer_ticks() - fault_last >= 150) {
            fault_last = timer_ticks();
            if (vigil_inject_fault()) {
                ser_str("INJ,tick="); ser_num(timer_ticks()); ser_str("\n");
                inj_count++;
                fault_left--;
            } else {
                fault_left = 0;                           /* baseline not locked: stop */
            }
        }

        /* replay: 56 days x 7 ticks = about 3.9 s */
        if (replay_on && timer_ticks() - replay_last >= 7) {
            replay_last += 7;
            vigil_on_reading('h', replay_hr[replay_day]);
            draw_replay();
            if (++replay_day >= 56) { replay_on = 0; replay_done = 1; draw_status(); }
        }

        /* scrub once per second, log every repair */
        if (timer_ticks() - last_scrub >= 100) {
            last_scrub += 100;
            if (!replay_on && !replay_done) draw_vitals();
            if (vigil_scrub(timer_ticks()) > 0) {
                fix_count++;
                draw_event();
                int ec; uint32_t eb, eg;
                if (vigil_event_active(timer_ticks(), &ec, &eb, &eg)) {
                    char ch[2] = { 'A' + ec, 0 };
                    ser_str("EVT,tick="); ser_num(timer_ticks());
                    ser_str(",copy=");    ser_str(ch);
                    ser_str(",bad=");     ser_num(eb);
                    ser_str(",good=");    ser_num(eg);
                    ser_str(",repaired=1\n");
                }
            }
        }

        /* heartbeat square: toggles 4x per second, only while the main loop is alive */
        if (timer_ticks() - last_blink >= 25) {
            last_blink += 25;
            blink_on ^= 1;
            gfx_fill_rect(20, 20, 48, 48, blink_on ? 0x00FFFFFF : 0x00000000);
        }

        int bc; uint32_t bb, bg;
        int on = vigil_event_active(timer_ticks(), &bc, &bb, &bg);
        int d  = vigil_take_dirty();
        if (on != banner_on || d) {
            banner_on = on;
            draw_status();
        }
        __asm__ volatile ("hlt");
    }
}
 