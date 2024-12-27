#ifndef _ROTATION_H_
#define _ROTATION_H_
#include "gadget.h"

#define ROTATION_PRECISION 30
#define ROTATION_FULL (1<<ROTATION_PRECISION)
#define ROTATION_HALF (1<<(ROTATION_PRECISION-1))
#define ROTATION_MASK ((1<<ROTATION_PRECISION)-1)

extern uint32_t rotation_zero;
extern bool rotation_stopped;
extern uint32_t rotation_period_raw;
extern uint32_t rotation_period;
extern bool rotation_lock;
extern int32_t rotation_drift;

void rotation_init(void);
uint32_t rotation_current_angle(void);


#endif
