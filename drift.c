#include "drift.h"

void baseline_add(baseline_t *b, int32_t sample, uint32_t lock_after){
	if (b->locked_flag) return;
	b->running_sum += sample;
	b->sample_count++;
	if (b->sample_count >= lock_after){
		b->locked_mean = b->running_sum / b->sample_count;
		b->locked_flag = 1;
	}
}

int baseline_locked(const baseline_t *b){
	return b->locked_flag;
}

int32_t drift_change(const vital_cfg_t *cfg, int32_t baseline, int32_t current){
	if (baseline <= 0) return 0;
	int change = current - baseline;
	if (cfg->bad_is_up == 0) change = -change;
	if (cfg->is_percent) return (change * 100) / baseline;
	return change;
}
