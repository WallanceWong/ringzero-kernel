#include "tmr.h"

void tmr_write(tmr_u32 *t, uint32_t v)
{
    t->c[0] = v;
    t->c[1] = v;
    t->c[2] = v;
}

uint32_t tmr_read(const tmr_u32 *t, tmr_status *st)
{
    uint32_t a = t->c[0], b = t->c[1], c = t->c[2];   /* read each copy once */
    uint32_t voted = (a & b) | (b & c) | (a & c);

	tmr_status s;
	if (a == b && b == c)                    s = TMR_OK;
	else if (a != b && b != c && a != c)     s = TMR_LOST;
	else                                     s = TMR_OUTVOTED;

	if (st) *st = s;
	return voted;
}

tmr_status tmr_scrub(tmr_u32 *t)
{
    tmr_status s;
    uint32_t voted = tmr_read(t, &s);

    if (s == TMR_OUTVOTED) tmr_write(t, voted);

    return s;
}

void tmr_inject(tmr_u32 *t, int copy, uint32_t mask)
{
    if (copy == 0 || copy == 1 || copy == 2) t->c[copy] ^= mask;
}