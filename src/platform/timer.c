#include "timer.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#include "rammel.h"

uint32_t timer_frame_count = 0;
uint32_t timer_frame_time = 0;
uint32_t timer_delta_time = 100;

[[maybe_unused]] timespec_t timer_start, timer_frame_curr, timer_frame_prev, timer_prof;


void timer_init() {
    clock_gettime(CLOCK_REALTIME, &timer_start);
    timer_frame_curr = timer_frame_prev = timer_start;
}

void timer_tick() {
    ++timer_frame_count;

    clock_gettime(CLOCK_REALTIME, &timer_frame_curr);
    int ms_elapsed = timer_diff_timespec_ms(&timer_frame_curr, &timer_frame_prev);
    timer_frame_prev = timer_frame_curr;

    timer_frame_time = timer_diff_timespec_ms(&timer_frame_curr, &timer_start);
    
    timer_delta_time = clamp(ms_elapsed, 1, 100);
}

void timer_sleep_until(timer_since_t offset, uint32_t ms) {
    timespec_t since;

    switch (offset) {
        case TIMER_SINCE_START: since = timer_start; break;
        default:
        case TIMER_SINCE_TICK:  since = timer_frame_curr; break;
    }
    
    timer_add_timespec_ms(&since, ms);
    clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &since, NULL);
}
