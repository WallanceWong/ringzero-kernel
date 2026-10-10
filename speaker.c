#include "io.h"

void speaker_on(unsigned int freq)
{
    unsigned short divider = 1193182 / freq;   /* the PIT base clock */
    outb(0x43, 0xB6);                       /* channel 2, lo/hi byte, square wave */
    outb(0x42, divider & 0xFF);             /* channel 2's data port */
    outb(0x42, (divider >> 8) & 0xFF);
    outb(0x61, inb(0x61) | 0x03);           /* open the gate + enable the speaker */
}

void speaker_off(void)
{
    outb(0x61, inb(0x61) & 0xFC);           /* clear those two bits */
}