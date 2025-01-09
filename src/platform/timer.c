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

[[maybe_unused]] struct timespec timer_start, timer_frame_curr, timer_frame_prev, timer_prof;

static int32_t diff_timespec_ms(struct timespec to, struct timespec from) {
    struct timespec diff;

    if ((to.tv_nsec - from.tv_nsec) < 0) {
        diff.tv_sec = to.tv_sec - from.tv_sec - 1;
        diff.tv_nsec = 1000000000 + to.tv_nsec - from.tv_nsec;
    } else {
        diff.tv_sec = to.tv_sec - from.tv_sec;
        diff.tv_nsec = to.tv_nsec - from.tv_nsec;
    }

    return (diff.tv_sec * 1000) + (diff.tv_nsec / 1000000);
}

static void add_timespec_ms(struct timespec* time, int ms) {
    time->tv_nsec += ms * 1000000;

    while (time->tv_nsec >= 1000000000) {
        time->tv_nsec -= 1000000000;
        time->tv_sec += 1;
    }
}

void timer_init() {

    clock_gettime(CLOCK_REALTIME, &timer_start);
    timer_frame_curr = timer_frame_prev = timer_start;
}

void timer_tick() {
    ++timer_frame_count;

    clock_gettime(CLOCK_REALTIME, &timer_frame_curr);
    int ms_elapsed = diff_timespec_ms(timer_frame_curr, timer_frame_prev);
    timer_frame_prev = timer_frame_curr;

    timer_frame_time = diff_timespec_ms(timer_frame_curr, timer_start);
    
    timer_delta_time = clamp(ms_elapsed, 1, 100);
}

void timer_sleep_until(timer_since_t offset, uint32_t ms) {
    struct timespec since;

    switch (offset) {
        case TIMER_SINCE_START: since = timer_start; break;
        default:
        case TIMER_SINCE_TICK:  since = timer_frame_curr; break;
    }
    
    add_timespec_ms(&since, ms);
    clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &since, NULL);
}
