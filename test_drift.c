#include <stdio.h>
#include "drift.h"

int main(void) {
    baseline_t b = {0};
    int32_t s[] = {70, 72, 74, 90};
    for (int i = 0; i < 4; i++) baseline_add(&b, s[i], 3);
    printf("locked=%d mean=%d\n", baseline_locked(&b), (int)b.locked_mean);
    return 0;
}