#include "vigil.h"
#include "tmr.h"

#define LOCK_AFTER 10

/* settings, from our threshold table */
static const vital_cfg_t cfg_hr = {1, 10, 25, 1};
static const vital_cfg_t cfg_spo2 = {0,  3,  6, 1};
static const vital_cfg_t cfg_temp = {1,  8, 15, 0};

/* state: one baseline, one counter and one level per vital */
static baseline_t    base_hr, base_spo2, base_temp;
static drift_state_t st_hr, st_spo2, st_temp;
static level_t       lvl_hr, lvl_spo2, lvl_temp;
static tmr_u32       tmr_hr, tmr_spo2, tmr_temp;
static int           dirty;

/* last repair event (for the on-screen banner) */
static int          ev_copy = -1;           /* which copy was bad; -1 = no event yet */
static uint32_t     ev_bad, ev_good;
static unsigned int ev_tick;

static int fault_copy = 0;                  /* which copy the next fault hits */

/* ---- HR rolling window: median of the last 7 readings ---- */
static int32_t hr_win[7];
static unsigned int hr_n;                 /* readings seen so far */

static void hr_push(int32_t v) { hr_win[hr_n % 7] = v; hr_n++; }

static int32_t hr_median(void)            /* only valid when hr_n >= 7 */
{
    int32_t t[7];
    for (int i = 0; i < 7; i++) t[i] = hr_win[i];
    for (int i = 1; i < 7; i++) {                 /* insertion sort */
        int32_t k = t[i]; int j = i - 1;
        while (j >= 0 && t[j] > k) { t[j + 1] = t[j]; j--; }
        t[j + 1] = k;
    }
    return t[3];
}

void vigil_on_reading(char kind, int32_t value)
{
    if (kind == 'h') hr_push(value);
    const vital_cfg_t *cfg;
    baseline_t        *b;
    drift_state_t     *st;
    level_t           *lvl;
    tmr_u32           *tm;

    switch (kind) {
    case 'h':
        cfg = &cfg_hr;   b = &base_hr;   st = &st_hr;   lvl = &lvl_hr;   tm = &tmr_hr;
        break;
    case 'o':
        cfg = &cfg_spo2; b = &base_spo2; st = &st_spo2; lvl = &lvl_spo2; tm = &tmr_spo2;
        break;
    case 't':
        cfg = &cfg_temp; b = &base_temp; st = &st_temp; lvl = &lvl_temp; tm = &tmr_temp;
        break;
    default:
        return;                              /* any other letter: ignore it */
    }

    if (!baseline_locked(b)) {
        baseline_add(b, value, LOCK_AFTER);
        if (baseline_locked(b)) {
            tmr_write(tm, b->locked_mean);   /* store the new baseline in 3 copies */
	    dirty =1;
        }
        return;
    }

    tmr_status ts;
    baseline_t vb = *b;                      /* a local copy of the struct */
    vb.locked_mean = tmr_read(tm, &ts);      /* use the voted baseline */
    int32_t use = (kind == 'h' && hr_n >= 7) ? hr_median() : value;
    level_t now = drift_update(cfg, &vb, st, use);
    if (now != *lvl) {
        *lvl = now;
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

uint32_t vigil_hr_copy(int i)                /* raw copy i (0, 1 or 2) of the HR baseline */
{
    if (i >= 0 && i < 3) return tmr_hr.c[i];
    else return 0;
}

uint32_t vigil_hr_voted(void)
{
    return tmr_read(&tmr_hr, 0);
}

int vigil_inject_fault(void)
{
    if (!baseline_locked(&base_hr))
        return 0;
    tmr_inject(&tmr_hr, fault_copy, 0x80);
    fault_copy = (fault_copy + 1) % 3;
    dirty = 1;
    return 1;
}

int vigil_scrub(unsigned int now)
{
    int repaired = 0;

    /* STEP 1: look at the HR copies and take notes, BEFORE repairing */
    uint32_t voted = vigil_hr_voted();
    for (int i = 0; i < 3; i++) {
        if (vigil_hr_copy(i) != voted) {
            ev_copy = i;
            ev_bad  = vigil_hr_copy(i);
            ev_good = voted;
            ev_tick = now;
        }
    }

    /* STEP 2: repair */
    tmr_u32 *stores[3] = { &tmr_hr, &tmr_spo2, &tmr_temp };
    for (int i = 0; i < 3; i++) {
        if (tmr_scrub(stores[i]) == TMR_OUTVOTED)
            repaired++;
    }

    if (repaired > 0) dirty = 1;
    return repaired;
}

int vigil_event_active(unsigned int now, int *copy, uint32_t *bad, uint32_t *good)
{
    if (ev_copy < 0) return 0;
    if (now - ev_tick >= 300) return 0;      /* the banner shows for 3 seconds */
    *copy = ev_copy;
    *bad  = ev_bad;
    *good = ev_good;
    return 1;
}

void vigil_reset(void)
{
    hr_n = 0;
    base_hr = (baseline_t){0};  base_spo2 = (baseline_t){0};  base_temp = (baseline_t){0};
    st_hr   = (drift_state_t){0}; st_spo2 = (drift_state_t){0}; st_temp = (drift_state_t){0};
    lvl_hr = lvl_spo2 = lvl_temp = LVL_GREEN;
    tmr_write(&tmr_hr, 0); tmr_write(&tmr_spo2, 0); tmr_write(&tmr_temp, 0);
    ev_copy = -1;
    dirty = 1;
}