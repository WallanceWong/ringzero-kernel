#include "io.h"
#include "vigil.h"
#include "replay_data.h"

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
int kb_pop(void);
void sensor_feed(char c);
void gfx_putc(char c);
unsigned int timer_ticks(void);

static void fmt3(char *out, uint32_t v)         /* 72 -> "072", over 999 -> "???" */
{
    if (v > 999) { out[0] = out[1] = out[2] = '?'; out[3] = 0; return; }
    out[0] = '0' + (v / 100);
    out[1] = '0' + ((v / 10) % 10);
    out[2] = '0' + (v % 10);
    out[3] = 0;
}

static void draw_tmr(void)
{
    char num[4];
    uint32_t vote = vigil_hr_voted();

    gfx_fill_rect(450, 244, 340, 48, 0x00101820);            /* clear the panel */
    gfx_draw_string(450, 244, "HR BASELINE", 0x00FFFFFF, 0x00101820);

    for (int i = 0; i < 3; i++) {
        uint32_t cv = vigil_hr_copy(i);
        unsigned int fg = (cv != vote) ? 0x00FF4040 : 0x00FFFFFF;
        char lab[3] = { 'A' + i, ':', 0 };
        fmt3(num, cv);
        gfx_draw_string(450 + i * 80,      260, lab, fg, 0x00101820);
        gfx_draw_string(450 + i * 80 + 16, 260, num, fg, 0x00101820);
    }

    fmt3(num, vote);
    gfx_draw_string(450,      276, "VOTE:", 0x00FFFFFF, 0x00101820);
    gfx_draw_string(450 + 48, 276, num,     0x00FFFFFF, 0x00101820);
}

static void draw_event(void)
{
    int copy;
    uint32_t bad, good;
    char num[4];
    const unsigned int bg = 0x00101820;

    gfx_fill_rect(450, 300, 340, 56, bg);              /* clear the banner area */

    if (!vigil_event_active(timer_ticks(), &copy, &bad, &good))
        return;

    char one[2] = { 'A' + copy, 0 };
    gfx_draw_string(450, 300, "FAULT: COPY ", 0x00FF4040, bg);
    gfx_draw_string(546, 300, one,            0x00FF4040, bg);
    gfx_draw_string(554, 300, " = ",          0x00FF4040, bg);
    fmt3(num, bad);
    gfx_draw_string(578, 300, num,            0x00FF4040, bg);

    gfx_draw_string(450, 316, "OUTVOTED -> ", 0x00FFFFFF, bg);
    fmt3(num, good);
    gfx_draw_string(546, 316, num,            0x00FFFFFF, bg);

    gfx_draw_string(450, 332, "REPAIRED",     0x0000FF00, bg);
}

static void draw_status(void)
{
    level_t lvl = vigil_level();
    unsigned int color;
    const char *name;

    switch (lvl) {
    case LVL_GREEN:
        color = 0x0000C000;
        name  = "GREEN";
        break;
    case LVL_AMBER:
        color = 0x00FFBF00;
        name  = "AMBER";
        break;
    case LVL_RED:
        color = 0x00C00000;
        name  = "RED  ";
        break;
    default:
        color = 0x0000C000;
        name  = "GREEN";
        break;
    }
    gfx_fill_rect(450, 100, 200, 100, color);
    gfx_draw_string(450, 220, name, 0x00FFFFFF, 0x00101820);
    draw_tmr();
    draw_event();
}

static void draw_sim(int on, int day)
{
    const unsigned int bg = 0x00101820;
    char wk[3] = { '0' + (day / 7 + 1), 0, 0 };
    gfx_fill_rect(450, 380, 340, 32, bg);
    if (!on) return;
    gfx_draw_string(450, 380, "SIMULATED REPLAY", 0x00FFBF00, bg);
    gfx_draw_string(450, 396, "WEEK ", 0x00FFFFFF, bg);
    gfx_draw_string(490, 396, wk, 0x00FFFFFF, bg);
    gfx_draw_string(498, 396, "/8", 0x00FFFFFF, bg);
}

int kmain(unsigned int *mbi)
{
    unsigned int last_scrub = 0;
    int banner_on = 0;
    int replay_on = 0, replay_day = 0;
    unsigned int replay_last = 0;

    serial_init(0x3F8);
    gfx_init(mbi);
    gfx_fill_rect(0, 0, 800, 600, 0x00101820);       /* dark background */
    gfx_draw_string(100, 220, "ringzero-kernel", 0x00FFFFFF, 0x00101820);

    gdt_init();
    idt_init();
    pic_remap();
    pit_init(100);
    outb(0x21, 0xFC);        /* enable timer IRQ0 and keyboard IRQ1 */
    __asm__ volatile ("sti");

    draw_status();                       /* initial block + panel */

    for (;;) {
        int c;
        while ((c = kb_pop()) != -1) {
            if (c == '\t') {
                vigil_inject_fault();
            } else if (c == 'g') {                /* DEBUG: instant baseline lock */
                for (int n = 0; n < 10; n++) {
                    sensor_feed('h'); sensor_feed('0'); sensor_feed('7');
                    sensor_feed('2'); sensor_feed('\n');
                }
	    } else if (c == 'r') {                        /* start 8-week simulated replay */
 	   	    vigil_reset();
 		    replay_on = 1; replay_day = 0; replay_last = timer_ticks();
		    draw_sim(1, 0);
            } else {
                gfx_putc((char)c);
                sensor_feed((char)c);
            }
        }

	if (replay_on && timer_ticks() - replay_last >= 7) {   /* 56 days x 7 ticks = about 3.9 s */
		replay_last += 7;
		vigil_on_reading('h', replay_hr[replay_day]);
    		draw_sim(1, replay_day);
    		if (++replay_day >= 56) { replay_on = 0; draw_status(); }
	}

        if (timer_ticks() - last_scrub >= 100) {
            last_scrub += 100;
            vigil_scrub(timer_ticks());
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