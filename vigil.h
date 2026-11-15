#ifndef VIGIL_H
#define VIGIL_H

#include <stdint.h>
#include "drift.h"

void     vigil_on_reading(char kind, int32_t value);
level_t  vigil_level(void);
level_t  vigil_vital_level(char kind);
int      vigil_take_dirty(void);
int      vigil_inject_fault(void);
int      vigil_scrub(unsigned int now);
uint32_t vigil_hr_copy(int i);
uint32_t vigil_hr_voted(void);
int      vigil_event_active(unsigned int now, int *copy, uint32_t *bad, uint32_t *good);
void vigil_reset(void);

#endif