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

level_t drift_update(const vital_cfg_t *cfg, const baseline_t *b, drift_state_t *st, int32_t current){
	if (!b->locked_flag) return LVL_GREEN;
	int32_t change = drift_change(cfg, b->locked_mean, current);
	if (change >= cfg->amber){
		st->over_count++;
	} else{
    		st->over_count = 0;
    		return LVL_GREEN;
	}
	if (st->over_count >= SUSTAIN_N){
		if (change >= cfg->red){
			return LVL_RED;
		} else{
			return LVL_AMBER;
		}
	} else return LVL_GREEN;
}
