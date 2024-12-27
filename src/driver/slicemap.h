#ifndef _SLICEMAP_H_
#define _SLICEMAP_H_
#include "gadget.h"


#define SLICE_COUNT 360
#define SLICE_QUADRANT (SLICE_COUNT / 4)
#define SLICE_WRAP(slice) ((slice) % (SLICE_COUNT))

#if SLICE_COUNT <= 256
    typedef uint8_t slice_index_t;
#else
    typedef uint16_t slice_index_t;
#endif


typedef struct {
    slice_index_t slice;
    uint8_t column;
} slice_polar_t;

typedef struct {
    voxel_index_t x, y;
} voxel_2D_t;

typedef enum {
    SLICE_BRIGHTNESS_UNIFORM,
    SLICE_BRIGHTNESS_BOOSTED,
    SLICE_BRIGHTNESS_UNLIMITED
} slice_brightness_t;


extern voxel_2D_t slice_map[SLICE_COUNT][PANEL_WIDTH][PANEL_COUNT];
extern float eccentricity[2];

void slicemap_init(slice_brightness_t brightness);


#endif
