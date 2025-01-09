#ifndef _TIMER_H_
#define _TIMER_H_

#include <stdint.h>

typedef enum {
    TIMER_SINCE_START,
    TIMER_SINCE_TICK
} timer_since_t;

extern uint32_t timer_frame_count;
extern uint32_t timer_frame_time;
extern uint32_t timer_delta_time;

void timer_init();
void timer_tick();
void timer_sleep_until(timer_since_t offset, uint32_t ms);

#endif

