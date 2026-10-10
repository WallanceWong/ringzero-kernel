#include "io.h"

int serial_write(unsigned short com, char *buf, unsigned int len);
int fb_write(char *buf, unsigned int len);
void speaker_off(void);
extern unsigned int sensor_hr, sensor_spo2, sensor_temp;
void kb_push(char c);

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

static void serial_hex(unsigned char v)
{
    char *d = "0123456789ABCDEF";
    char out[3];
    out[0] = d[v >> 4];
    out[1] = d[v & 0x0F];
    out[2] = ' ';
    serial_write(0x3F8, out, 3);
}

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

void interrupt_handler(struct cpu_state *s)
{
    if (s->int_no == 3) {
        serial_write(0x3F8, "int!\n", 5);
    } else if (s->int_no == 33) {
        unsigned char sc = inb(0x60);
        serial_hex(sc);
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

