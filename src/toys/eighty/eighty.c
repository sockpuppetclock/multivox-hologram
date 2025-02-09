#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <sched.h>
#include <time.h>
#include <linux/joystick.h>
#include <poll.h>
#include <dirent.h>
#include <termios.h>

#include "array.h"
#include "mathc.h"
#include "rammel.h"
#include "input.h"
#include "graphics.h"
#include "model.h"
#include "timer.h"
#include "tubeface.h"
#include "scooter.h"
#include "grid.h"

voxel_double_buffer_t* volume_buffer;

void main_init(void) {
    timer_init();

    tubeface_init();
    grid_init();
    scooter_init();
}

void main_update(float dt) {    
    input_update();
    scooter_update(dt);
}

void main_draw(pixel_t* volume) {
    tubeface_draw(volume);
    grid_draw(volume);
    scooter_draw(volume);
}

int main(int argc, char** argv) {
    if (!voxel_buffer_map()) {
        exit(1);
    }

    main_init();

    input_set_nonblocking();

    for (int ch = 0; ch != 27; ch = getchar()) {
        timer_tick();

        pixel_t* volume = voxel_buffer_get(VOXEL_BUFFER_BACK);
        voxel_buffer_clear(volume);

        main_update((float)timer_delta_time * 0.001f);
        main_draw(volume);

        voxel_buffer_swap();

        timer_sleep_until(TIMER_SINCE_TICK, 30);
    }

    voxel_buffer_unmap();

    return 0;
}
