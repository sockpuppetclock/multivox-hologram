#include "scooter.h"

#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "rammel.h"
#include "input.h"
#include "grid.h"
#include "mathc.h"

typedef struct {
    int controller;
    int position[2];
    int direction;
    int steer;
    float trail;
    float speed;
} scooter_t;

static const int direction[4][2] = {{1,0}, {0,1}, {-1,0}, {0,-1}};
#define SCOOTER_AI -1

static scooter_t scooters[2];

void scooter_init(void) {
    memset(scooters, 0, sizeof(scooters));

    scooters[0].controller = SCOOTER_AI;
    grid_start_pose(0, scooters[0].position, &scooters[0].direction);
    scooters[0].speed = 4;

    scooters[1].controller = SCOOTER_AI;
    grid_start_pose(1, scooters[1].position, &scooters[1].direction);
    scooters[1].speed = 4;
}

void scooter_update(float dt) {
    if (input_get_button(0, BUTTON_MENU, BUTTON_PRESSED)) {
        grid_init();
        scooter_init();
    }

    for (int i = 0; i < count_of(scooters); ++i) {
        scooter_t* scooter = &scooters[i];

        if (scooter->controller != SCOOTER_AI) {
            if (input_get_button(scooter->controller, BUTTON_LB, BUTTON_PRESSED)) {
                scooter->steer = 1;
            }
            if (input_get_button(scooter->controller, BUTTON_RB, BUTTON_PRESSED)) {
                scooter->steer = -1;
            }
        } else {
            if (scooter->trail + scooter->speed * dt >= 0.5f) {
                int next[2];
                vec2i_add(next, scooter->position, direction[scooter->direction & 3]);

                int probes[4];
                grid_probe(next, probes);
                if (probes[scooter->direction & 3] < 10) {
                    if (probes[(scooter->direction + 1) & 3] > probes[(scooter->direction - 1) & 3]) {
                        if (probes[(scooter->direction + 1) & 3] >= probes[scooter->direction & 3]) {
                            scooter->steer = 1;
                        }
                    } else {
                        if (probes[(scooter->direction - 1) & 3] >= probes[scooter->direction & 3]) {
                            scooter->steer = -1;
                        }
                    }
                }
            }
        }
    }

    for (int i = 0; i < count_of(scooters); ++i) {
        scooter_t* scooter = &scooters[i];

        float was = scooter->trail;
        scooter->trail += scooter->speed * dt;
        bool crossed = (was <= 0 && scooter->trail > 0);

        while (scooter->trail >= 0.5f) {
            grid_mark(i, scooter->position, 0);

            vec2i_add(scooter->position, scooter->position, direction[scooter->direction & 3]);

            if (grid_occupied(scooter->position)) {
                scooter->trail = 0;
                scooter->speed = 0;
            } else {
                grid_mark(i, scooter->position, 1 << ((scooter->direction + 2) & 3));
                scooter->trail -= 1.0f;
            }
        }

        if (crossed)  {
            scooter->direction = (scooter->direction + scooter->steer) & 3;
            scooter->steer = 0;
            grid_mark(i, scooter->position, 1 << (scooter->direction & 3));
        }
    }
}

void scooter_draw(pixel_t* volume) {

    for (int i = 0; i < count_of(scooters); ++i) {
        scooter_t* scooter = &scooters[i];

        if (scooter->speed > 0) {
            int vox[3];
            float pos[2] = {
                scooter->position[0] + direction[scooter->direction & 3][0] * scooter->trail,
                scooter->position[1] + direction[scooter->direction & 3][1] * scooter->trail
            };
            grid_vox_pos(vox, pos);

            const int length = 0;
            pixel_t colour = RGBPIX(128,128,128);

            int inf[2] = {max(         0, min(vox[0], vox[0] - direction[scooter->direction & 3][0] * length) - 1),
                          max(         0, min(vox[1], vox[1] - direction[scooter->direction & 3][1] * length) - 1)};
            int sup[2] = {min(VOXELS_X-1, max(vox[0], vox[0] - direction[scooter->direction & 3][0] * length) + 1),
                          min(VOXELS_Y-1, max(vox[1], vox[1] - direction[scooter->direction & 3][1] * length) + 1)};
            
            for (int y = inf[1]; y <= sup[1]; ++y) {
                for (int x = inf[0]; x <= sup[0]; ++x) {
                    for (int z = vox[2]+1; z < vox[2]+4; ++z) {
                        volume[VOXEL_INDEX(x, y, z)] = colour;
                    }
                }
            }
        }
    }
}

