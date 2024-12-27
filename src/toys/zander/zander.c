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
#include "gadget.h"
#include "graphics.h"
#include "model.h"
#include "ship.h"
#include "terrain.h"
#include "particles.h"
#include "objects.h"

volume_double_buffer_t* volume_buffer;

int8_t height_map[VOXELS_Y][VOXELS_X][2];

float world_scale = 8.0f;
vec3_t world_position = {.x=0, .y=0, .z=0};

float delta_scale = 0.0f;

float control_thrust = 0.0f;
vec2_t control_stick = {.x=0, .y=0};
bool control_fire = false;
uint32_t time_framecount = 0;
uint32_t time_now_ms = 0;

void world_from_voxel(float* position, const int32_t* voxel) {
    position[0] = ((voxel[0] - (VOXELS_X / 2)) / world_scale) + world_position.x;
    position[1] = ((voxel[1] - (VOXELS_Y / 2)) / world_scale) + world_position.y;
    position[2] = ((voxel[2]                 ) / world_scale) + world_position.z;
}

void input_update(void) {
    static int js = -1;

    if (js == -1) {
        js = open("/dev/input/js0", O_RDONLY | O_NONBLOCK);
    } else {
        ssize_t events;
        struct js_event event;
        while ((events = read(js, &event, sizeof(event))) > 0) {
            if (event.type == JS_EVENT_BUTTON) {
                switch (event.number) {
                    case BUTTON_RB:
                        control_fire = event.value;
                        break;
                    default:
                        break;
                }
            } else if (event.type == JS_EVENT_AXIS) {
                switch (event.number) {
                    case AXIS_LS_X: {
                        control_stick.x =  (float)event.value * (1.0f / 32767.0f);
                    } break;
                    case AXIS_LS_Y: {
                        control_stick.y = -(float)event.value * (1.0f / 32767.0f);
                    } break;

                    case AXIS_RT: {
                        float delta = (float)(event.value + 32000) / 64000.0f;
                        control_thrust = min(max(0.0f, delta), 1.0f);
                    } break;

                    case AXIS_RS_Y: {
                        delta_scale = (float)event.value * (1.0f / 32767.0f);
                    } break;

                    default:
                        break;
                }

            }
        }
        if (events < 0 && errno != EAGAIN) {
            close(js);
            js = -1;
        }
    }
}

void main_init(void) {
    objects_init();
    terrain_init();
}

void main_update(float dt) {    
    input_update();

    world_scale *= 1.0f + delta_scale * dt;

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

int32_t diff_timespec_ms(struct timespec to, struct timespec from) {
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

void add_timespec_ms(struct timespec* time, int ms) {
    time->tv_nsec += ms * 1000000;

    while (time->tv_nsec >= 1000000000) {
        time->tv_nsec -= 1000000000;
        time->tv_sec += 1;
    }
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

    [[maybe_unused]] struct timespec time_start, time_curr, time_prev, time_prof;
    clock_gettime(CLOCK_REALTIME, &time_start);
    time_curr = time_prev = time_start;

    main_init();

    input_set_nonblocking();

    for (int ch = 0; ch != 27; ch = getchar()) {
        uint8_t page = !volume_buffer->page;
        pixel_t* volume = volume_buffer->volume[page];

        clock_gettime(CLOCK_REALTIME, &time_curr);
        int ms_elapsed = diff_timespec_ms(time_curr, time_prev);
        time_prev = time_curr;

        time_now_ms = diff_timespec_ms(time_curr, time_start);
        
        float dt = clamp(ms_elapsed * 0.001f, 0.001f, 0.1f);

        memset(volume, 0, sizeof(volume_buffer->volume[page]));

        main_update(dt);
        main_draw(volume);
        volume_buffer->page = page;

        /*clock_gettime(CLOCK_REALTIME, &time_prof);
        int busy_ms = diff_timespec_ms(time_prof, time_prev);
        printf("%d\n", busy_ms);*/

        add_timespec_ms(&time_curr, 30);
        clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &time_curr, NULL);

        ++time_framecount;
    }

    munmap(volume_buffer, sizeof(*volume_buffer));
    close(fd);

    return 0;
}
