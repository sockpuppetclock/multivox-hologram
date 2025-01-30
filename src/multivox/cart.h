#ifndef _CART_H_
#define _CART_H_

#include "voxel.h"

typedef struct {
    const char* command;
    const char* arguments;
    const char* workingdir;
    const char* environment;

    pixel_t colour;

    const char* cartpath;
    pixel_t * voxel_shot[4];
} cart_t;

typedef enum {
    CART_ACTION_NONE,
    CART_ACTION_PAUSE,
    CART_ACTION_EJECT,
    CART_ACTION_FAIL
} cart_action_t;

void cart_grab_voxshot(cart_t* cart, const pixel_t* volume);
void cart_save_voxshot(cart_t* cart);
bool cart_load(cart_t* cart, char* filename);
void cart_draw(cart_t* cart, pixel_t* volume, float slot_angle);

#endif
