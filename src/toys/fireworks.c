#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "rammel.h"
#include "input.h"
#include "voxel.h"


void draw_sphere(int x, int y, int z, int radius, uint8_t colour) {
    pixel_t* volume = voxel_buffer_get(VOXEL_BUFFER_FRONT);

    int xi = (x - radius < 0) ? -x : -radius;
    int xs = (x + radius >= VOXELS_X) ? (VOXELS_X-1 - x) : radius;
    int yi = (y - radius < 0) ? -y : -radius;
    int ys = (y + radius >= VOXELS_Y) ? (VOXELS_Y-1 - y) : radius;
    int zi = (z - radius < 0) ? -z : -radius;
    int zs = (z + radius >= VOXELS_Z) ? (VOXELS_Z-1 - z) : radius;

    int rosq = radius * radius / 4;

    for (int xx = xi; xx <= xs; ++xx) {
        for (int yy = yi; yy <= ys; ++yy) {
            for (int zz = zi; zz <= zs; ++zz) {
                int lsq = xx*xx + yy*yy + zz*zz;
                if (lsq <= rosq) {
                    volume[VOXEL_INDEX(x+xx, y+yy, z+zz)] = colour;
                }
            }
        }
    }
}

void draw_sphereaa(int x, int y, int z, int radius, uint8_t colour) {
    pixel_t* volume = voxel_buffer_get(VOXEL_BUFFER_FRONT);

    int xi = (x - radius < 0) ? -x : -radius;
    int xs = (x + radius >= VOXELS_X) ? (VOXELS_X-1 - x) : radius;
    int yi = (y - radius < 0) ? -y : -radius;
    int ys = (y + radius >= VOXELS_Y) ? (VOXELS_Y-1 - y) : radius;
    int zi = (z - radius < 0) ? -z : -radius;
    int zs = (z + radius >= VOXELS_Z) ? (VOXELS_Z-1 - z) : radius;

    int rosq = radius * radius;
    int risq = (radius-1) * (radius-1);
    uint8_t ci = (colour&0b10010010)>>1;

    for (int xx = xi; xx <= xs; ++xx) {
        for (int yy = yi; yy <= ys; ++yy) {
            for (int zz = zi; zz <= zs; ++zz) {
                int lsq = xx*xx + yy*yy + zz*zz;
                if (lsq <= rosq) {
                    uint c = lsq > risq ? ci : colour;
                    volume[VOXEL_INDEX(x+xx, y+yy, z+zz)] = c;
                }
            }
        }
    }
}

typedef struct {
    int x, y, z, w;
} vec4i_t;

typedef struct {
    float x, y, z;
} vec3_t;

typedef struct {
    vec3_t position;
    vec3_t velocity;
    int radius;
    uint8_t colour;
} ball_t;


#define TRAIL_LEN 256
ball_t balls[256];
vec4i_t trails[count_of(balls)][TRAIL_LEN];
int trail_index = 0;


#define ROCKET_RADIUS 2
#define SPARK_RADIUS 1

void spawn_rocket(int i) {
    balls[i].position.x = (float)(rand()&31)+48;
    balls[i].position.y = (float)(rand()&31)+48;
    balls[i].position.z = 0;

    balls[i].velocity.x = (float)((rand()&31)-15) * 0.001f;
    balls[i].velocity.y = (float)((rand()&31)-15) * 0.001f;
    balls[i].velocity.z = (float)((rand()&3)+2) * (VOXELS_Z / 2560.0f);

    balls[i].radius = ROCKET_RADIUS;
    
    uint8_t c = (rand()%6)+1;
    balls[i].colour = ((c&4)<<5) | ((c&2)<<3) | ((c&1)<<1);
}

void spawn_sparks(float x, float y, float z, uint8_t colour, int count) {
    for (uint b = 0; b < count_of(balls) && count > 0; ++b) {
        if (balls[b].radius == 0) {
            balls[b].position.x = x;
            balls[b].position.y = y;
            balls[b].position.z = z;
            balls[b].radius = SPARK_RADIUS;
            balls[b].colour = colour;
            balls[b].velocity.x = (float)((rand()&31)-15) * 0.003f;
            balls[b].velocity.y = (float)((rand()&31)-15) * 0.003f;
            balls[b].velocity.z = (float)((rand()&31)-15) * 0.003f;
            --count;
        }
    }
}

int count_rockets() {
    int count = 0;
    for (uint b = 0; b < count_of(balls); ++b) {
        if (balls[b].radius == ROCKET_RADIUS) {
            ++count;
        }
    }
    return count;
}

void update_ball(int b) {
    ball_t* ball = &balls[b];

    if (ball->radius <= 0) {
        return;
    }

    if (ball->radius == ROCKET_RADIUS && ball->velocity.z <= -0.001f) {
        ball->radius = 0;
        spawn_sparks(ball->position.x, ball->position.y, ball->position.z, ball->colour, 32);
        return;
    }

    ball->position.x += ball->velocity.x;
    ball->position.y += ball->velocity.y;
    ball->position.z += ball->velocity.z;

    ball->velocity.z -= 0.0002f;


    if ( ball->position.x < 0
      ||ball->position.x >= VOXELS_X
      ||ball->position.y < 0
      ||ball->position.y >= VOXELS_Y
      ||ball->position.z < 0
      ||ball->position.z >= VOXELS_Z) {
        ball->radius = 0;
        if (count_rockets() < 2) {
            spawn_rocket(b);
        }
        return;
    }

    int x = ball->position.x;
    int y = ball->position.y;
    int z = ball->position.z;
    trails[b][trail_index].x = x;
    trails[b][trail_index].y = y;
    trails[b][trail_index].z = z;
    trails[b][trail_index].w = ball->radius;
    draw_sphere(x, y, z, ball->radius, ball->colour);
}



int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    if (!voxel_buffer_map()) {
        exit(1);
    }

    for (int i = 0; i < 8; ++i) {
        balls[i].position.x = 0;
        balls[i].position.y = 0;
        balls[i].position.z = (float)(rand()&(VOXELS_Z-1));
        balls[i].velocity.x = 0;
        balls[i].velocity.y = 0;
        balls[i].velocity.z = 0;
        balls[i].colour = 255;
        balls[i].radius = SPARK_RADIUS;
    }

    pixel_t* volume = voxel_buffer_get(VOXEL_BUFFER_FRONT);
    voxel_buffer_clear(volume);

    input_set_nonblocking();

    for (int ch = 0; ch != 27; ch = getchar()) {
        trail_index = (trail_index+1) & (TRAIL_LEN-1);

        for (uint b = 0; b < count_of(balls); ++b) {
            if (trails[b][trail_index].w > 0) {
                draw_sphere(trails[b][trail_index].x, trails[b][trail_index].y, trails[b][trail_index].z, trails[b][trail_index].w, 0);
                trails[b][trail_index].w = 0;
            }
            update_ball(b);
        }

        usleep(1000);
    }

    voxel_buffer_unmap();

    return 0;
}




