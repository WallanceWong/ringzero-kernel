/* astro.c - the ASTRONAUT health console (keyboard only).
 * A non-blocking state machine: astro_key() handles one key event,
 * astro_tick() is called every main-loop pass (PIT time), astro_draw()
 * repaints only when something changed. Nothing here ever waits. */
#include "astro.h"

void gfx_fill_rect(unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned int color);
void gfx_draw_string(unsigned int x, unsigned int y, const char *s, unsigned int fg, unsigned int bg);
void gfx_draw_string_scaled(unsigned int x, unsigned int y, const char *s,
                            unsigned int fg, unsigned int bg, unsigned int scale);
unsigned int timer_ticks(void);

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

typedef enum {
    S_CLOSED, S_MENU, S_GUIDE
} astate_t;

static astate_t st = S_CLOSED;
static int dirty = 0;               /* console needs a full repaint */
static int guide_page = 0;
static unsigned int last_strip = 0;

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

/* ---- drawing ---- */
static void draw_header(void)
{
    gfx_fill_rect(0, 0, 800, 88, C_HDR);
    gfx_fill_rect(0, 88, 800, 2, C_ACC);
    gfx_draw_string_scaled(100, 16, "ASTRONAUT CONSOLE", C_TXT, C_HDR, 3);
    gfx_draw_string(100, 66, "A1-37 VIGIL  |  your own health check  |  keyboard only", C_DIM, C_HDR);
    gfx_draw_string(8, 70, "HEARTBEAT", C_DIM, C_HDR);
    gfx_fill_rect(508, 20, 280, 48, C_AMBER);
    gfx_draw_string_scaled(512, 28, "VITALS: SIMULATED", C_BG, C_AMBER, 2);
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
    put_str(s, &n, "SIMULATED VITALS  HR "); fmt3(t, sensor_hr); put_str(s, &n, t);
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

static void draw_menu(void)
{
    gfx_draw_string(20, 124, "MENU", C_ACC, C_BG);
    gfx_draw_string_scaled(40, 148, "1  Health guide", C_TXT, C_BG, 2);
    gfx_draw_string_scaled(40, 184, "2  Check-in", C_TXT, C_BG, 2);
    gfx_draw_string_scaled(40, 220, "3  Reaction test", C_TXT, C_BG, 2);
    gfx_draw_string_scaled(40, 256, "4  Memory test", C_TXT, C_BG, 2);
    gfx_draw_string_scaled(40, 292, "5  Mental math", C_TXT, C_BG, 2);
    draw_footer("1-5: choose    Esc: close console");
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

static void draw_all(void)
{
    gfx_fill_rect(0, 90, 800, 480, C_BG);
    draw_header();
    draw_strip();
    switch (st) {
    case S_MENU:  draw_menu();  break;
    case S_GUIDE: draw_guide(); break;
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

void astro_key(char c)
{
    if (st == S_CLOSED) return;
    if (st == S_MENU) {
        if (c == 27) { st = S_CLOSED; return; }
        if (c == '1') { st = S_GUIDE; guide_page = 0; dirty = 1; }
        return;
    }
    if (st == S_GUIDE) {
        if (c == 27) { st = S_MENU; dirty = 1; }
        else if (c == 0x12 || c == 'n') { guide_page = (guide_page + 1) % GUIDE_PAGES; dirty = 1; }
        else if (c == 0x11 || c == 'p') { guide_page = (guide_page + GUIDE_PAGES - 1) % GUIDE_PAGES; dirty = 1; }
    }
}

void astro_tick(unsigned int now)
{
    if (st == S_CLOSED) return;
    if (now - last_strip >= 100) {            /* keep the simulated-vitals strip fresh */
        last_strip += 100;
        draw_strip();
    }
}

void astro_draw(void)
{
    if (st == S_CLOSED || !dirty) return;
    dirty = 0;
    draw_all();
}

int astro_scrub(unsigned int now) { (void)now; return 0; }
int astro_inject_fault(void) { return 0; }
int astro_checkin_due(unsigned int now) { (void)now; return 0; }
int astro_attention(unsigned int now, char *out, unsigned int cap, int *level)
{
    (void)now; (void)out; (void)cap; (void)level;
    return 0;
}
int astro_take_dirty(void) { return 0; }
