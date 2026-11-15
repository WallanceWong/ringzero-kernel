#ifndef ASTRO_H
#define ASTRO_H

#include <stdint.h>

#define GUIDE_PAGES 8
#define GUIDE_PARTS 5

/* ---- guide.c ---- */
int         guide_page_count(void);
const char *guide_title(int p);
const char *guide_source(int p);
const char *guide_part_name(int k);
const char *guide_text(int p, int k);
int         draw_wrapped(unsigned int x, unsigned int y, const char *s, unsigned int cols,
                         unsigned int fg, unsigned int bg, unsigned int lh);

/* ---- astro.c: the astronaut console (non-blocking state machine) ---- */
int  astro_is_open(void);
void astro_open(void);
void astro_key(char c);                 /* one key event */
void astro_tick(unsigned int now);      /* call every main-loop pass */
void astro_draw(void);                  /* repaints only when something changed */
int  astro_scrub(unsigned int now);     /* TMR scrub of the astronaut channels; returns repairs */
int  astro_inject_fault(void);          /* 1 if a fault was injected */
int  astro_checkin_due(unsigned int now);
int  astro_attention(unsigned int now, char *out, unsigned int cap, int *level);
int  astro_take_dirty(void);            /* 1 if main-screen text must be repainted */
int  astro_demo_active(void);           /* 1 once the SIMULATED demo history was loaded */

#endif
