#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "rammel.h"
#include "gpio.h"
#include "gadget.h"

#include "rotation.h"


static uint32_t sync_prev = 0;
static uint32_t rotation_angle = 0;
static int32_t rotation_delta = 256;
static int sync_level = 1;
static uint32_t tick_prev = 0;
static uint32_t rotation_history[8];

uint32_t rotation_zero = ROTATION_FULL / 360 * ROTATION_ZERO;
bool rotation_stopped = true;
uint32_t rotation_period_raw = 0;
uint32_t rotation_period = 1<<26;
bool rotation_lock = true;
int32_t rotation_drift = 0;

int compare_ints(const void *a, const void *b) {
    return *((int*)a) - *((int*)b);
}

#ifdef SYNC_PULSE_UNEQUAL
static uint32_t median_period() {
    // treat the rising and falling edges separately
    static uint32_t sorted[2][count_of(rotation_history[0])];
    memcpy(sorted, rotation_history, sizeof(sorted));
    for (int i = 0; i < 2; ++i) {
        qsort(sorted[i], count_of(sorted[i]), sizeof(*sorted[i]), compare_ints);
    }
    
    return (sorted[0][(count_of(rotation_history[0])-1)/2] + sorted[0][count_of(rotation_history[0])/2]
          + sorted[1][(count_of(rotation_history[0])-1)/2] + sorted[1][count_of(rotation_history[0])/2]) / 2;
}
#else
static uint32_t median_period() {
    static uint32_t sorted[count_of(rotation_history)];
    memcpy(sorted, rotation_history, sizeof(sorted));
    qsort(sorted, count_of(sorted), sizeof(*sorted), compare_ints);
    
    return (sorted[(count_of(rotation_history)-1)/2] + sorted[count_of(rotation_history)/2]);
}
#endif

uint32_t rotation_current_angle(void) {
    uint32_t tick_curr = *timer_uS;
    uint32_t elapsed = tick_curr - sync_prev;
    
    static uint32_t current = 0;

    int sync = gpio_get_pin(SPIN_SYNC);
    if (sync != sync_level) {
        sync_level = sync;
        
        sync_prev = tick_curr;
        rotation_period_raw = elapsed * 2;
        if (elapsed > 10000 && elapsed < 10000000) {
            if (++current >= count_of(rotation_history)) {
                current = 0;
            }
            rotation_history[current] = elapsed;
            rotation_period = median_period();
            rotation_period = max(10000, rotation_period);

            rotation_delta = ROTATION_FULL / rotation_period;
            if (rotation_lock) {
                int recentre = ((int32_t)((rotation_angle + (!sync * ROTATION_HALF)) & ROTATION_MASK) - ROTATION_HALF) >> 17;
                recentre = clamp(recentre, -rotation_delta / 16, rotation_delta / 16);
                rotation_delta -= recentre;
            }
        }
    }

    rotation_stopped = (elapsed > 1000000);

    uint32_t dtick = (tick_curr - tick_prev);
    tick_prev = tick_curr;

    uint32_t delta = dtick * rotation_delta;

    rotation_angle = (rotation_angle + delta) & ROTATION_MASK;

    rotation_zero = (rotation_zero + ROTATION_FULL + (dtick * rotation_drift)) & ROTATION_MASK;

    return (rotation_angle + rotation_zero) & ROTATION_MASK;
}

void rotation_init(void) {
    rotation_stopped = true;
}


