#include <stdio.h>
#include "tmr.h"

int main(void) {
    tmr_u32 t;
    tmr_status st;
    uint32_t v;

    tmr_write(&t, 72);
    v = tmr_read(&t, &st);
    printf("%u %d\n", v, st);            /* 72 0 */

    t.c[1] = 200;
    v = tmr_read(&t, &st);
    printf("%u %d\n", v, st);            /* 72 1 */

    t.c[0] = 1; t.c[1] = 2; t.c[2] = 4;
    v = tmr_read(&t, &st);
    printf("%u %d\n", v, st);            /* 0 2 */

    tmr_write(&t, 72);
    tmr_inject(&t, 1, 0x80);
    v = tmr_read(&t, &st);
    printf("%u %d\n", v, st);            /* 72 1 */

    tmr_status found = tmr_scrub(&t);
    printf("scrub=%d\n", found);         /* scrub=1 */

    v = tmr_read(&t, &st);
    printf("%u %d\n", v, st);            /* 72 0 */
    return 0;
}