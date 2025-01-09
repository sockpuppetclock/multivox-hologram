#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>

#include "array.h"
#include "mathc.h"
#include "rammel.h"
#include "input.h"
#include "voxel.h"
#include "graphics.h"
#include "model.h"
#include "timer.h"
#include "carousel.h"


void main_init(void) {
    timer_init();
    carousel_init();
}

void main_update(float dt) {    
    input_update();
    carousel_update(dt);
}

void main_draw(pixel_t* volume) {
    carousel_draw(volume);
}

int main(int argc, char** argv) {

    if (!voxel_buffer_map()) {
        exit(1);
    }

    input_set_nonblocking();

    main_init();

    for (int ch = 0; ch != 27; ch = getchar()) {
        pixel_t* volume = voxel_buffer_get(VOXEL_BUFFER_BACK);
        voxel_buffer_clear(volume);

        timer_tick();

        main_update((float)timer_delta_time * 0.001f);
        main_draw(volume);

        voxel_buffer_swap();

        timer_sleep_until(TIMER_SINCE_TICK, 30);
    }

    voxel_buffer_unmap();

    return 0;
}




