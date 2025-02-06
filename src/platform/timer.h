#ifndef _TIMER_H_
#define _TIMER_H_

#include <stdint.h>
#include <time.h>

typedef enum {
    TIMER_SINCE_START,
    TIMER_SINCE_TICK
} timer_since_t;

extern uint32_t timer_frame_count;
extern uint32_t timer_frame_time;
extern uint32_t timer_delta_time;

typedef struct timespec timespec_t;

static inline int32_t timer_diff_timespec_ms(const timespec_t* to, const timespec_t* from) {
    struct timespec diff;

    if ((to->tv_nsec - from->tv_nsec) < 0) {
        diff.tv_sec = to->tv_sec - from->tv_sec - 1;
        diff.tv_nsec = 1000000000 + to->tv_nsec - from->tv_nsec;
    } else {
        diff.tv_sec = to->tv_sec - from->tv_sec;
        diff.tv_nsec = to->tv_nsec - from->tv_nsec;
    }

    return (diff.tv_sec * 1000) + (diff.tv_nsec / 1000000);
}

static inline void timer_add_timespec_ms(timespec_t* time, int ms) {
    time->tv_nsec += ms * 1000000;

    while (time->tv_nsec >= 1000000000) {
        time->tv_nsec -= 1000000000;
        time->tv_sec += 1;
    }
}

static inline timespec_t timer_time_now(void) {
    timespec_t timenow;
    clock_gettime(CLOCK_REALTIME, &timenow);
    return timenow;
}

static inline int32_t timer_elapsed_ms(timespec_t* timer) {
    timespec_t timenow;
    clock_gettime(CLOCK_REALTIME, &timenow);
    int32_t elapsed = timer_diff_timespec_ms(&timenow, timer);
    *timer = timenow;
    return elapsed;
}

void timer_init();
void timer_tick();
void timer_sleep_until(timer_since_t offset, uint32_t ms);

#endif

