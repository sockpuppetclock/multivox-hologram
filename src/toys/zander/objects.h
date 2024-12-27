#ifndef _OBJECTS_H_
#define _OBJECTS_H_

#include "gadget.h"
#include <stdbool.h>

void objects_init(void);
bool objects_hit_and_destroy(float* position);
void objects_update(float dt);
void objects_draw(pixel_t* volume);

#endif
