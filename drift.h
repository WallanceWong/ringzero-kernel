#ifndef DRIFT_H
#define DRIFT_H
#define SUSTAIN_N 3
#include <stdint.h>

typedef enum {
    LVL_GREEN,
    LVL_AMBER,
    LVL_RED
} level_t;

typedef struct{
    int bad_is_up; /* 1 = rising is bad, 0 = falling is bad */
    int32_t amber;
    int32_t red;
    int is_percent;
}vital_cfg_t;

typedef struct{
    uint32_t running_sum;
    uint32_t sample_count;
    uint32_t locked_mean;
    int locked_flag;
} baseline_t;

typedef struct{
    uint32_t over_count;
}drift_state_t;

void    baseline_add(baseline_t *b, int32_t sample, uint32_t lock_after);
int     baseline_locked(const baseline_t *b);
level_t drift_update(const vital_cfg_t *cfg, const baseline_t *b,
                     drift_state_t *st, int32_t current);
int32_t drift_change(const vital_cfg_t *cfg, int32_t baseline, int32_t current);
#endif
