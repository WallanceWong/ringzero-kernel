#include "font.h"
static unsigned int fb_addr, fb_pitch, fb_w, fb_h;

void gfx_init(unsigned int *mbi)
{
    fb_addr  = mbi[22];     /* the same indexes as in the table above */
    fb_pitch = mbi[24];
    fb_w     = mbi[25];
    fb_h     = mbi[26];
}

void gfx_put_pixel(unsigned int x, unsigned int y, unsigned int color)
{
    if (x >= fb_w || y >= fb_h)
        return;                                        /* never draw off screen */
    volatile unsigned int *p =
        (volatile unsigned int *)(fb_addr + y * fb_pitch + x * 4);
    *p = color;
}

void gfx_fill_rect(unsigned int x, unsigned int y,
                   unsigned int w, unsigned int h, unsigned int color)
{
    for (unsigned int j = 0; j < h; j++)
        for (unsigned int i = 0; i < w; i++)
            gfx_put_pixel(x + i, y + j, color);
}

void gfx_draw_char(unsigned int x, unsigned int y, char c,
                   unsigned int fg, unsigned int bg)
{
    if (c < FONT_FIRST || c >= FONT_FIRST + FONT_COUNT)
        c = '?';                                  /* unknown character */
    const unsigned char *glyph = font8x16[c - FONT_FIRST];  /* which index? */

    for (unsigned int row = 0; row < FONT_H; row++) {
        for (unsigned int col = 0; col < FONT_W; col++) {
            if (glyph[row] & (0x80 >> col))       /* is this pixel set? */
                gfx_put_pixel(x + col, y + row, fg);
            else
                gfx_put_pixel(x + col, y + row, bg);
        }
    }
}

void gfx_draw_string(unsigned int x, unsigned int y, const char *s,
                     unsigned int fg, unsigned int bg)
{
    while (*s) {
        gfx_draw_char(x, y, *s, fg, bg);
        x += FONT_W;                                /* advance one character width */
        s++;
    }
}

static unsigned int cur_x = 20, cur_y = 300;

void gfx_putc(char c)
{
    if (c == '\n') {
        cur_x = 20;
        cur_y += FONT_H;                    /* one text line down */
    } else {
        gfx_draw_char(cur_x, cur_y, c, 0x00FFFFFF, 0x00101820);
        cur_x += FONT_W;                     /* next character cell */
        if (cur_x + FONT_W > fb_w) {      /* past the right edge of the screen? */
            cur_x = 20;
            cur_y += FONT_H;
        }
    }
}