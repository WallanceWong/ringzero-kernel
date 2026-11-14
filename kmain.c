#include "io.h"
#include "vigil.h"

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
	color = 0x00C0C000;
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
}

int kmain(unsigned int *mbi)
{
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

    draw_status();                       /* initial GREEN block */

    for (;;) {
        int c;
        while ((c = kb_pop()) != -1) {
            gfx_putc((char)c);
            sensor_feed((char)c);
        }
        if (vigil_take_dirty())
            draw_status();
        __asm__ volatile ("hlt");
    }
}
