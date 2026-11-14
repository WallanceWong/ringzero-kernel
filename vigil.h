#ifndef VIGIL_H                 /* 1. guard name, like DRIFT_H */
#define VIGIL_H

#include "drift.h"               /* gives you level_t */

void    vigil_on_reading(char kind, int32_t value);
level_t vigil_level(void);
level_t vigil_vital_level(char kind);
int     vigil_take_dirty(void);
void vigil_inject_fault(void);
int  vigil_scrub(void);

#endif                            /* 2. nothing else needed */