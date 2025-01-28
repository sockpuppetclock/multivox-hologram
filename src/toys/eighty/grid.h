#ifndef _GRID_H_
#define _GRID_H_

#include "voxel.h"

void grid_init(void);
void grid_start_pose(int id, int* cell, int* direction);
void grid_vox_pos(int* vox, const float* pos);
bool grid_occupied(const int* cell);
void grid_probe(int* cell, int* probes);
void grid_mark(int id, const int* cell, uint8_t walls);
void grid_draw(pixel_t* volume);

#endif
