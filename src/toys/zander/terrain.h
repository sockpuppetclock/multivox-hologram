#ifndef _TERRAIN_H_
#define _TERRAIN_H_

#include <stdbool.h>
#include "voxel.h"

static inline bool terrain_is_water(float altitude) {return altitude <= 1e-2f;}

float terrain_get_altitude_raw(float x, float y);
float terrain_get_altitude(float x, float y);
void terrain_init(void);
void terrain_draw(pixel_t* volume);

#endif
