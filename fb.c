#define FB_ADDRESS 0x000B8000   /* VGA text memory */
void fb_move_cursor(unsigned short pos);

/* Write one character cell.
 * i  = cell index (0..1999)    c  = the character
 * fg = foreground colour (0-15)  bg = background colour (0-15)
 */
void fb_write_cell(unsigned int i, char c, unsigned char fg, unsigned char bg)
{
    volatile char *fb = (volatile char *) FB_ADDRESS;
    fb[i * 2]     = c;                    /* byte 1: the character */
    fb[i * 2 + 1] = ((bg & 0x0F) << 4) | (fg & 0x0F); /* byte 2: the colour */
}

#define FB_COLS 80
#define FB_ROWS 25

void fb_clear(void)
{
    unsigned int i;
    for (i = 0; i < FB_COLS * FB_ROWS; i++) {
        fb_write_cell(i, ' ', 7, 0);   /* which character blanks a cell? */
    }
}

static unsigned int fb_pos = 0;   /* where the next character goes (cell index) */

static void fb_scroll(void)
{
    volatile char *fb = (volatile char *) FB_ADDRESS;
    unsigned int i;
    for (i = 0; i < (FB_ROWS - 1) * FB_COLS * 2; i++) {   /* bytes per cell? */
        fb[i] = fb[i + FB_COLS * 2];                      /* same, one row ahead */
    }
    for (i = (FB_ROWS - 1) * FB_COLS; i < FB_ROWS * FB_COLS; i++) {
        fb_write_cell(i, ' ', 7, 0);                        /* blank the last row */
    }
    fb_pos = (FB_ROWS - 1) * FB_COLS;                           /* start of the last row */
}

int fb_write(char *buf, unsigned int len)
{
    unsigned int i;
    for (i = 0; i < len; i++) {
    if (buf[i] == '\n') {
        fb_pos = (fb_pos / FB_COLS + 1) * FB_COLS;
    } else {
        fb_write_cell(fb_pos, buf[i], 7, 0);
        fb_pos++;
    }
    if (fb_pos >= FB_ROWS * FB_COLS) {   /* ran past the last row? */
        fb_scroll();
    }
}
    fb_move_cursor(fb_pos);
    return len;                              /* how many characters were written? */
}

