#include "io.h"

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

int kmain(unsigned int *mbi)
{
    (void)mbi;

    serial_init(0x3F8);
    gfx_init(mbi);
    gfx_fill_rect(0, 0, 800, 600, 0x00101820);       /* dark background */
    gfx_fill_rect(100, 100, 200, 100, 0x00FF0000);   /* red rectangle */
    gfx_draw_string(100, 220, "A1-37 VIGIL", 0x00FFFFFF, 0x00101820);
    serial_hex32(mbi[0]);
    serial_hex32(mbi[22]);
    serial_hex32(mbi[24]);
    serial_hex32(mbi[25]);
    serial_hex32(mbi[26]);

    gdt_init();
    idt_init();
    pic_remap();
    pit_init(100);
    outb(0x21, 0xFC);        /* enable timer IRQ0 and keyboard IRQ1 */
    speaker_on(1000);
    __asm__ volatile ("sti");

    for (;;) {
        int c;
        while ((c = kb_pop()) != -1) {
            gfx_putc((char)c);
            sensor_feed((char)c);
        }
        __asm__ volatile ("hlt");
    }
}