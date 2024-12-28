#ifndef _OBJECTS_H_
#define _OBJECTS_H_

#include <stdbool.h>
#include "voxel.h"

void objects_init(void);
bool objects_hit_and_destroy(float* position);
void objects_update(float dt);
void objects_draw(pixel_t* volume);

#endif
