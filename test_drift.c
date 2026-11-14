#include <stdio.h>
#include "drift.h"

int main(void) {
    /* baseline test */
    baseline_t b = {0};
    int32_t s[] = {70, 72, 74, 90};
    for (int i = 0; i < 4; i++) baseline_add(&b, s[i], 3);
    printf("locked=%d mean=%d\n", baseline_locked(&b), (int)b.locked_mean);

    /* drift_change test */
    vital_cfg_t hr   = {1, 15, 25, 1};
    vital_cfg_t spo2 = {0, 3, 6, 1};
    vital_cfg_t temp = {1, 8, 15, 0};
    printf("hr=%d spo2=%d spo2up=%d temp=%d\n",
        (int)drift_change(&hr, 72, 83),
        (int)drift_change(&spo2, 98, 94),
        (int)drift_change(&spo2, 98, 99),
        (int)drift_change(&temp, 366, 375));

   /* drift_update test */
    baseline_t hb = {0};
    for (int i = 0; i < 3; i++) baseline_add(&hb, 72, 3);
    drift_state_t st = {0};
    int32_t seq[] = {83, 84, 85, 74, 91, 91, 91};
    for (int i = 0; i < 7; i++)
        printf("%d ", (int)drift_update(&hr, &hb, &st, seq[i]));
    printf("\n");

    return 0;
}