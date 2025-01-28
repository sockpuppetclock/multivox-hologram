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

    int fd = shm_open("/rotovox_double_buffer", O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open");
        exit(1);
    }

    volume_buffer = mmap(NULL, sizeof(*volume_buffer), PROT_WRITE, MAP_SHARED, fd, 0);
    if (volume_buffer == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    volume_buffer->bits_per_channel = 2;

    main_init();

    input_set_nonblocking();

    for (int ch = 0; ch != 27; ch = getchar()) {
        uint8_t page = !volume_buffer->page;
        pixel_t* volume = volume_buffer->volume[page];

        timer_tick();

        memset(volume, 0, sizeof(volume_buffer->volume[page]));

        main_update((float)timer_delta_time * 0.001f);
        main_draw(volume);
        volume_buffer->page = page;

        timer_sleep_until(TIMER_SINCE_TICK, 30);
    }

    munmap(volume_buffer, sizeof(*volume_buffer));
    close(fd);

    return 0;
}
