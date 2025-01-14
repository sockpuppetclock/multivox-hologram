#ifndef _CART_H_
#define _CART_H_

#include "voxel.h"

typedef struct {
    const char* command;
    const char* arguments;
    const char* workingdir;
    const char* environment;

    pixel_t colour;
} cart_t;

typedef enum {
    CART_ACTION_NONE,
    CART_ACTION_PAUSE,
    CART_ACTION_EJECT,
    CART_ACTION_FAIL
} cart_action_t;

#endif
