#ifndef TMR_H
#define TMR_H

#include <stdint.h>

typedef struct {
    volatile uint32_t c[3];         /* the three copies */
} tmr_u32;

typedef enum {
    TMR_OK,                         /* all three agree */
    TMR_OUTVOTED,
    TMR_LOST,
} tmr_status;

void        tmr_write(tmr_u32 *t, uint32_t v);
uint32_t    tmr_read(const tmr_u32 *t, tmr_status *st);
tmr_status  tmr_scrub(tmr_u32 *t);
void        tmr_inject(tmr_u32 *t, int copy, uint32_t mask);

#endif