#include "io.h"

void pit_init(unsigned int hz)
{
    unsigned short divider = 1193182 / hz;   /* the PIT's base frequency */
    outb(0x43, 0x36);                     /* ch0, lo/hi byte, square wave */
    outb(0x40, divider & 0xFF);           /* low byte first */
     outb(0x40, (divider >> 8) & 0xFF);                     /* then the high byte */
}