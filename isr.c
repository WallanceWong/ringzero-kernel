#include "io.h"

int serial_write(unsigned short com, char *buf, unsigned int len);
int fb_write(char *buf, unsigned int len);
void speaker_off(void);
void kb_push(char c);
void gfx_draw_string(unsigned int x, unsigned int y, const char *s, unsigned int fg, unsigned int bg);
void gfx_fill_rect(unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned int color);

struct cpu_state {
    unsigned int edi;
    unsigned int esi;
    unsigned int ebp;
    unsigned int esp;
    unsigned int ebx;
    unsigned int edx;
    unsigned int ecx;
    unsigned int eax;
    unsigned int int_no;
    unsigned int err_code;
    unsigned int eip;
    unsigned int cs;
    unsigned int eflags;
};

static const char scancode_ascii[128] = {
    0,  0, '1','2','3','4','5','6','7','8','9','0', 0, 0, 0, 0,        /* 0x00-0x0F */
    'q','w','e','r','t','y','u','i','o','p', 0, 0, '\n', 0, 'a','s',    /* 0x10-0x1F */
    'd', 'f', 'g', 'h', 'j', 'k', 'l', 0, 0, 0, 0, 0, 'z', 'x', 'c', 'v',	/* 0x20-0x2F*/
    'b', 'n', 'm', 0, 0, 0, 0, 0, 0, ' ', 0, 0, 0, 0, 0, 0,		/* 0x30-0x3F*/

};

static volatile unsigned int ticks = 0;
unsigned int timer_ticks(void)
{
    return ticks;
}

static void put_hex(unsigned int v, unsigned int x, unsigned int y)
{
    char buf[9];
    for (int i = 0; i < 8; i++) {
        int shift = (7 - i) * 4;                     /* bits per hex digit */
        buf[i] = "0123456789ABCDEF"[(v >> shift) & 0xF];
    }
    buf[8] = 0;
    gfx_draw_string(x, y, buf, 0x00FFFFFF, 0x00AA0000);
}

static void exception_screen(struct cpu_state *s)
{
    gfx_fill_rect(0, 0, 800, 600, 0x00AA0000);   /* red background */
    gfx_draw_string(20, 20, "EXCEPTION", 0x00FFFFFF, 0x00AA0000);
    put_hex(s->int_no, 20, 50);     /* the interrupt number */
    put_hex(s->eip, 20, 80);     /* the faulting EIP */
    for (;;) __asm__ volatile ("cli; hlt");
}

void interrupt_handler(struct cpu_state *s)
{
    if (s->int_no < 32) {
       exception_screen(s);
   }
     if (s->int_no == 33) {
	unsigned char sc = inb(0x60);
        if (!(sc & 0x80)) {
            char c = scancode_ascii[sc];
            if (c)
                kb_push(c);
        }
        outb(0x20, 0x20);
    } else if (s->int_no == 32) {
        ticks++;
        if (ticks % 100 == 0)
            serial_write(0x3F8, "tick\n", 5);
        outb(0x20, 0x20);
	if (ticks == 200)
            speaker_off();
    }
}

