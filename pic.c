#include "io.h"

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

void pic_remap(void)
{
    outb(PIC1_CMD,  0x11);   /* start init (master) */
    outb(PIC2_CMD,  0x11);   /* start init (slave)  */
    outb(PIC1_DATA, 0x20);   /* master vector offset: first IRQ becomes int ?  */
    outb(PIC2_DATA, 0x28);   /* slave vector offset: 8 after the master        */
    outb(PIC1_DATA, 0x04);   /* tell master: slave is on IRQ2 */
    outb(PIC2_DATA, 0x02);   /* tell slave: its cascade identity */
    outb(PIC1_DATA, 0x01);   /* 8086 mode */
    outb(PIC2_DATA, 0x01);
    outb(PIC1_DATA, 0xFF);   /* mask ALL IRQs for now */
    outb(PIC2_DATA, 0xFF);
}