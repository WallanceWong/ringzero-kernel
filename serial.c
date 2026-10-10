#include "io.h"

#define SERIAL_COM1_BASE            0x3F8
#define SERIAL_DATA_PORT(base)      (base)
#define SERIAL_FIFO_COMMAND_PORT(base) (base + 2)
#define SERIAL_LINE_COMMAND_PORT(base) (base + 3)
#define SERIAL_MODEM_COMMAND_PORT(base) (base + 4)
#define SERIAL_LINE_STATUS_PORT(base)  (base + 5)

void serial_init(unsigned short com)
{
    /* baud rate: tell the port the divisor comes next (DLAB) */
    outb(SERIAL_LINE_COMMAND_PORT(com), 0x80);        /* 0x80 */
    outb(SERIAL_DATA_PORT(com), 0x00);                /* divisor high byte */
    outb(SERIAL_DATA_PORT(com), 0x03);                /* divisor low byte  */

    /* line: 8 data bits, no parity, 1 stop bit */
    outb(SERIAL_LINE_COMMAND_PORT(com), 0x03);        /* 0x03 */

    /* FIFO: enable, clear both queues */
    outb(SERIAL_FIFO_COMMAND_PORT(com), 0xC7);	         /* 0xC7 */

    /* modem: RTS and DTR = 1 */
    outb(SERIAL_MODEM_COMMAND_PORT(com), 0x03);       /* 0x03 */
}

static int serial_is_transmit_fifo_empty(unsigned short com)
{
    return inb(SERIAL_LINE_STATUS_PORT(com)) & 0x20;
}

void serial_write_byte(unsigned short com, char c)
{
    while (!serial_is_transmit_fifo_empty(com)) {
    }
    outb(SERIAL_DATA_PORT(com), c);
}

int serial_write(unsigned short com, char *buf, unsigned int len)
{
    unsigned int i;
    for (i = 0; i < len; i++) {                  /* how many characters? */
        serial_write_byte(com, buf[i]);        /* which character? */
    }
    return len;                                  /* how many were written? */
}
void serial_hex32(unsigned int v)
{
    char *d = "0123456789ABCDEF";
    char out[9];
    for (int i = 0; i < 8; i++)
        out[i] = d[(v >> (28 - 4 * i)) & 0xF];
    out[8] = '\n';
    serial_write(0x3F8, out, 9);
}