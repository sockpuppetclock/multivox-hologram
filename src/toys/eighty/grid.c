#include "grid.h"

#include <string.h>
#include <math.h>

#include "mathc.h"
#include "timer.h"
#include "rammel.h"

enum {
    OCCUPANT_EMPTY,
    OCCUPANT_CORE,
    OCCUPANT_WALL,
    OCCUPANT_SCOOTER_0,
    OCCUPANT_SCOOTER_1
};

typedef struct {
    pixel_t colour;
    int height;
} occupant_t;

#define OCCUPANT_TYPES 8
static occupant_t occupants[OCCUPANT_TYPES] = {
    {.colour=0, .height=0},
    {.colour=RGBPIX(0,127,127), .height=2},
    {.colour=RGBPIX(255,127,0), .height=2},
    {.colour=RGBPIX(0,0,255), .height=8},
    {.colour=RGBPIX(128,127,0), .height=8},
};

#define GRID_FLOOR 6
#define GRID_WIDTH 32
#define GRID_HEIGHT 32
#define GRID_SPACING ((int)(VOXELS_X+VOXELS_Y)/(int)(GRID_WIDTH+GRID_HEIGHT))



typedef struct {
    uint32_t spawned;
    uint8_t occupant;
    uint8_t walls;
} cell_t;

static cell_t grid_map[GRID_HEIGHT][GRID_WIDTH];

static float ease_out_elastic(float v) {
    if (v <= 0) {
        return 0;
    }
    if (v >= 1) {
        return 1;
    }

    const float c4 = (2.0f * M_PI) / 3.0f;
    return powf(2.0f, -10.0f * v) * sinf((v * 10.0f - 0.75f) * c4) + 1.0f;   
}

void grid_init(void) {
    const int outer = sqr((GRID_WIDTH+GRID_HEIGHT)/2 - 2);
    const int inner = sqr(36/GRID_SPACING);

    memset(grid_map, 0, sizeof(grid_map));

    for (int y = 0; y < GRID_HEIGHT; ++y) {
        for (int x = 0; x < GRID_WIDTH; ++x) {
            
            int rsq4 = sqr((x*2)-(GRID_WIDTH-1)) + sqr((y*2)-(GRID_HEIGHT-1));

            if (rsq4 <= inner) {
                grid_map[y][x].occupant = OCCUPANT_CORE;
                grid_map[y][x].walls = 0xff;
            } else if (rsq4 >= outer) {
                grid_map[y][x].occupant = OCCUPANT_WALL;
                grid_map[y][x].walls = 0xff;
            }
        }
    }
}

void grid_start_pose(int id, int* cell, int* direction) {
    int start_pose[4][3] = {
        {GRID_WIDTH/2 - GRID_WIDTH/4,  GRID_HEIGHT/2, 1},
        {GRID_WIDTH/2 + GRID_WIDTH/4,  GRID_HEIGHT/2, 3},
        {GRID_WIDTH/2, GRID_HEIGHT/2 - GRID_HEIGHT/4, 2},
        {GRID_WIDTH/2, GRID_HEIGHT/2 + GRID_HEIGHT/4, 0},
    };

    vec2i_assign(cell, start_pose[id % count_of(start_pose)]);
    *direction = start_pose[id % count_of(start_pose)][2];
}

void grid_vox_pos(int* vox, const float* pos) {
    vox[0] = (int)((pos[0] - (GRID_WIDTH/2) ) * GRID_SPACING) + (VOXELS_X/2) + (GRID_SPACING/2);
    vox[1] = (int)((pos[1] - (GRID_HEIGHT/2)) * GRID_SPACING) + (VOXELS_Y/2) + (GRID_SPACING/2);
    vox[2] = GRID_FLOOR;
}

bool grid_occupied(const int* cell) {
    if ((uint)cell[0] >= GRID_WIDTH || (uint)cell[1] >= GRID_HEIGHT) {
        return true;
    }
    return (grid_map[cell[1]][cell[0]].occupant != 0);
}

void grid_probe(int* cell, int* probes) {
    int px = cell[0];
    int py = cell[1];

    memset(probes, 0, 4*sizeof(int));

    for (int x = px + 1; x < GRID_WIDTH && !grid_map[py][x].occupant; ++x) {
        ++probes[0];
    }
    for (int x = px - 1; x >= 0 && !grid_map[py][x].occupant; --x) {
        ++probes[2];
    }
    for (int y = py + 1; y < GRID_HEIGHT && !grid_map[y][px].occupant; ++y) {
        ++probes[1];
    }
    for (int y = py - 1; y >= 0 && !grid_map[y][px].occupant; --y) {
        ++probes[3];
    }
}

void grid_mark(int id, const int* cell, uint8_t walls) {
    if ((uint)cell[0] >= GRID_WIDTH || (uint)cell[1] >= GRID_HEIGHT) {
        return;
    }

    cell_t* grid = &grid_map[cell[1]][cell[0]];
    grid->spawned = timer_frame_time;
    grid->occupant = id + OCCUPANT_SCOOTER_0;
    grid->walls |= walls;
}

void grid_draw(pixel_t* volume) {
    const int z0 = GRID_FLOOR;
    const pixel_t basecol = RGBPIX(127,255,255);

    for (int y = 0; y < GRID_WIDTH; ++y) {
        for (int x = 0; x < GRID_HEIGHT; ++x) {
            cell_t* cell = &grid_map[y][x];

            if (cell->occupant) {
                occupant_t o = occupants[cell->occupant & (OCCUPANT_TYPES-1)];
                int height = o.height;
                if (cell->occupant >= OCCUPANT_SCOOTER_0) {
                    height *= ease_out_elastic((float)(timer_frame_time - cell->spawned) * 0.0001f) * 2.0f - 1.0f;
                }
                height = max(1, height);

                int vx = ((x - (GRID_WIDTH/2)) * GRID_SPACING) + (VOXELS_X/2) + (GRID_SPACING/2);
                int vy = ((y - (GRID_HEIGHT/2))* GRID_SPACING) + (VOXELS_Y/2) + (GRID_SPACING/2);

                if (cell->walls & 0x10) {
                    int vxi = vx - (GRID_SPACING/2 - 1);
                    int vxs = vx + (GRID_SPACING/2 - 1);
                    int vyi = vy - (GRID_SPACING/2 - 1);
                    int vys = vy + (GRID_SPACING/2 - 1);
                
                    for (int fvy = vyi; fvy <= vys; ++fvy) {
                        for (int fvx = vxi; fvx <= vxs; ++fvx) {
                            for (int z = 0; z < height; ++z) {
                                pixel_t colour = z ? o.colour : basecol;
                                volume[VOXEL_INDEX(fvx, fvy, z + z0)] = colour;
                            }
                        }
                    }
                } else {
                    if (cell->walls & 0x0f) {
                        for (int i = 0; i < GRID_SPACING/2; ++i) {
                            for (int z = 0; z < height; ++z) {
                                pixel_t colour = z ? o.colour : basecol;
                                if (cell->walls & 1) {
                                    volume[VOXEL_INDEX(vx+i, vy, z + z0)] = colour;
                                }
                                if (cell->walls & 2) {
                                    volume[VOXEL_INDEX(vx, vy+i, z + z0)] = colour;
                                }
                                if (cell->walls & 4) {
                                    volume[VOXEL_INDEX(vx-i-1, vy, z + z0)] = colour;
                                }
                                if (cell->walls & 8) {
                                    volume[VOXEL_INDEX(vx, vy-i-1, z + z0)] = colour;
                                }
                            }
                        }
                    }
                }
                
            }
        }
    }

}

