#include "vigil.h"
#include "tmr.h"

#define LOCK_AFTER 10

/* settings, from our threshold table */
static const vital_cfg_t cfg_hr   = {1, 15, 25, 1};
static const vital_cfg_t cfg_spo2 = {0,  3,  6, 1};
static const vital_cfg_t cfg_temp = {1,  8, 15, 0};

/* state: one baseline, one counter and one level per vital */
static baseline_t    base_hr, base_spo2, base_temp;
static drift_state_t st_hr, st_spo2, st_temp;
static level_t       lvl_hr, lvl_spo2, lvl_temp;
static tmr_u32 tmr_hr, tmr_spo2, tmr_temp;
static int           dirty;

void vigil_on_reading(char kind, int32_t value)
{
	const vital_cfg_t *cfg;
	baseline_t        *b;
	drift_state_t     *st;
	level_t           *lvl;
	tmr_u32           *tm;          /* ADD THIS LINE */

	switch (kind) {
	case 'h':
	    cfg = &cfg_hr;
	    b   = &base_hr;
	    st  = &st_hr;
	    lvl = &lvl_hr;
	    tm = &tmr_hr;
	    break;
	case 'o':
	    /* same four lines, using the spo2 names */
	    cfg = &cfg_spo2;
	    b   = &base_spo2;
	    st  = &st_spo2;
	    lvl = &lvl_spo2;
	    tm = &tmr_spo2;
	    break;
	case 't':
	    /* same four lines, using the temp names */
	    cfg = &cfg_temp;
	    b   = &base_temp;
	    st  = &st_temp;
	    lvl = &lvl_temp;
	    tm = &tmr_temp;
	    break;
	default:
	    return;          /* any other letter: ignore it */
	}

        if (!baseline_locked(b)) {
            baseline_add(b, value, LOCK_AFTER);
            if (baseline_locked(b)) {
                tmr_write(tm, b->locked_mean);   /* store the new baseline in 3 copies */
            }
            return;
        }

        tmr_status ts;
        baseline_t vb = *b;                      /* a local copy of the struct */
        vb.locked_mean = tmr_read(tm, &ts);      /* use the voted baseline */
        level_t now = drift_update(cfg, &vb, st, value);
	if (now != *lvl) {
   	 *lvl = now;        /* *lvl means "the thing lvl points at" */
  	  dirty = 1;
	}
}
	level_t vigil_vital_level(char kind)
	{
   	 switch (kind) {
   	 case 'h': return lvl_hr;
   	 case 'o': return lvl_spo2;
   	 case 't': return lvl_temp;
   	 default:  return LVL_GREEN;
   	 	}
	}

	level_t vigil_level(void)
	{
	    level_t worst = lvl_hr;

	    if (lvl_spo2 > worst) worst = lvl_spo2;

	     if (lvl_temp > worst) worst = lvl_temp;

	    return worst;
	}

	int vigil_take_dirty(void)
	{
	    int was = dirty;
	    dirty = 0;
 	    return was;
	}

static int fault_copy = 0;                    /* which copy the next fault hits */

void vigil_inject_fault(void)
{
    if(baseline_locked(&base_hr)) {
        tmr_inject(&tmr_hr, fault_copy, 0x80);
        fault_copy = (fault_copy + 1) % 3;  /* move fault_copy to the next copy: 0 -> 1 -> 2 -> 0 */
	dirty = 1;
    }
}

int vigil_scrub(void)
{
    int repaired = 0;
    tmr_u32 *stores[3] = { &tmr_hr, &tmr_spo2, &tmr_temp };

    for (int i = 0; i < 3; i++) {
        if (tmr_scrub(stores[i]) == TMR_OUTVOTED)
            repaired++;
    }
    if (repaired > 0) dirty = 1;  
    return repaired;
}

uint32_t vigil_hr_copy(int i)       /* raw copy i (0, 1 or 2) of the HR baseline */
{
     if (i >= 0 && i < 3) return tmr_hr.c[i];
     else return 0;
}

uint32_t vigil_hr_voted(void)
{
    return tmr_read(&tmr_hr, 0);
}