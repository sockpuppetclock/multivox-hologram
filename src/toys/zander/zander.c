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

#include "zander.h"

#include "array.h"
#include "mathc.h"
#include "rammel.h"
#include "input.h"
#include "graphics.h"
#include "model.h"
#include "ship.h"
#include "terrain.h"
#include "particles.h"
#include "objects.h"
#include "timer.h"

voxel_double_buffer_t* volume_buffer;

int8_t height_map[VOXELS_Y][VOXELS_X][2];

float world_scale = 8.0f;
vec3_t world_position = {.x=0, .y=0, .z=0};

void world_from_voxel(float* position, const int32_t* voxel) {
    position[0] = ((voxel[0] - (VOXELS_X / 2)) / world_scale) + world_position.x;
    position[1] = ((voxel[1] - (VOXELS_Y / 2)) / world_scale) + world_position.y;
    position[2] = ((voxel[2]                 ) / world_scale) + world_position.z;
}
void main_init(void) {
    timer_init();
    objects_init();
    terrain_init();
}

void main_update(float dt) {    
    input_update();

    world_scale *= 1.0f + input_get_axis(0, AXIS_RS_Y) * dt;

    ship_update(dt);

    world_position.x = ship_position.x;
    world_position.y = ship_position.y + (16.0f / world_scale);
    world_position.z = max(0.0f, ship_position.z - 56.0f / world_scale);

    particles_update(dt);
    objects_update(dt);

}

void main_draw(pixel_t* volume) {
    terrain_draw(volume);
    objects_draw(volume);
    particles_draw(volume);
    ship_draw(volume);
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

    volume_buffer->bpc = 2;

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
