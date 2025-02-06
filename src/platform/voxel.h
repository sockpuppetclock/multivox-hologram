#ifndef _VOXEL_H_
#define _VOXEL_H_

// all the gadget-specific stuff goes in gadget_gadgetname.h, and is selected via `cmake -DMULTIVOX_GADGET=gadgetname ..`

#include <stdint.h>
#include <stdbool.h>
#include "gadget.h"

#define R565(p) (((p)>>8) & 0xf8)
#define G565(p) (((p)>>3) & 0xfc)
#define B565(p) (((p)<<3) & 0xf8)
#define RGB565(r,g,b) ((((r)<<8)&0xf800) | (((g)<<3)&0x07e0) | (((b)>>3)&0x001f))

#define R332(p) ((((p)>>5)& 7)*36)
#define G332(p) ((((p)>>2)& 7)*36)
#define B332(p) (((p)     & 3)*85)
#define RGB332(r,g,b) (((r)&0xe0) | (((g)>>3)&0x1c) | (((b)>>6)&0x03))

#ifdef HIGH_COLOUR

typedef uint16_t pixel_t;

#define R_PIX(a) R565(a)
#define G_PIX(a) G565(a)
#define B_PIX(a) B565(a)

#define RGBPIX(r,g,b) RGB565(r,g,b)
#define HEXPIX(hex) RGB565(((int)(0x##hex & 0xFF0000)>>16), ((int)(0x##hex & 0xFF00)>>8), (int)(0x##hex & 0xFF))

#define R_MTH_BIT(p, b) (((p)>>(15-b))&1)
#define G_MTH_BIT(p, b) (((p)>>(10-b))&1)
#define B_MTH_BIT(p, b) (((p)>>( 4-b))&1)

#define R_THRESHOLD(p, t) (( (p)        & 0b1111100000000000) >= (t))
#define G_THRESHOLD(p, t) ((((p << 5))  & 0b1111110000000000) >= (t))
#define B_THRESHOLD(p, t) ((((p << 11)) & 0b1111100000000000) >= (t))

#else

typedef uint8_t pixel_t;

#define R_PIX(a) R332(a)
#define G_PIX(a) G332(a)
#define B_PIX(a) B332(a)

#define RGBPIX(r,g,b) RGB332(r,g,b)
#define HEXPIX(hex) RGB332(((int)(0x##hex & 0xFF0000)>>16), ((int)(0x##hex & 0xFF00)>>8), (int)(0x##hex & 0xFF))

#define R_MTH_BIT(p, b) (((p)>>(7-b))&1)
#define G_MTH_BIT(p, b) (((p)>>(4-b))&1)
#define B_MTH_BIT(p, b) (((p)>>(~b&1))&1)

#define R_THRESHOLD(p, t) (( (p)       & 0b11100000) >= (t))
#define G_THRESHOLD(p, t) ((((p << 3)) & 0b11100000) >= (t))
#define B_THRESHOLD(p, t) ((((p << 6)) & 0b11000000) >= (t))

#endif

#if (VOXELS_X <= 256) && (VOXELS_Y <= 256) && (VOXELS_Z <= 256)
    typedef uint8_t voxel_index_t;
#else
    typedef uint16_t voxel_index_t;
#endif

#if defined (VOXEL_INDEX_SPLIT)
static inline int VOXEL_INDEX(int x, int y, int z) {
    return (x*VOXEL_X_STRIDE + y*VOXEL_Y_STRIDE + (z&((VOXELS_Z/2)-1))*VOXEL_Z_STRIDE) * 2 + ((z/(VOXELS_Z/2))&1);
}
#define VOXEL_FIELD_STRIDE 1

#elif defined (VOXEL_INDEX_MORTON)
static inline int VOXEL_INDEX(int x, int y, int z) {
    x = (x | (x << 4)) & 0x0F0F0F0F0F0F0F0F;
    x = (x | (x << 2)) & 0x3333333333333333;
    x = (x | (x << 1)) & 0x5555555555555555;

    y = (y | (y << 4)) & 0x0F0F0F0F0F0F0F0F;
    y = (y | (y << 2)) & 0x3333333333333333;
    y = (y | (y << 1)) & 0x5555555555555555;

    int morton = x | (y << 1);
    return (morton + (z&(VOXELS_Z/2-1))*VOXEL_Z_STRIDE) * 2 + ((z / (VOXELS_Z/2)) & 1);
}
#else

#define VOXEL_INDEX(x,y,z) ((x)*VOXEL_X_STRIDE + (y)*VOXEL_Y_STRIDE + (z)*VOXEL_Z_STRIDE)
#define VOXEL_FIELD_STRIDE (PANEL_FIELD_HEIGHT * VOXEL_Z_STRIDE)
#endif

enum {
    VORTEX_BRIGHTNESS_UNIFORM =   0x0000,
    VORTEX_BRIGHTNESS_OVERDRIVE = 0x0001,
    VORTEX_BRIGHTNESS_SATURATE =  0x0002,
    VORTEX_BRIGHTNESS_MASK =      0x0003,
    VORTEX_DISABLE_PANEL_0 =      0x0004,
    VORTEX_DISABLE_PANEL_1 =      0x0008,
    VORTEX_DISABLE_TRAILS =       0x0010,
    VORTEX_STOP_AXIS_VERTICAL =   0x0020,
    VORTEX_ROTISSERIE =           0x0040
};

typedef struct {
    pixel_t volume[2][VOXELS_COUNT];
    uint8_t page;
    uint8_t bits_per_channel;
    uint16_t debug_flags;
    uint16_t revolutions_per_minute;
    uint16_t microseconds_per_frame;
} voxel_double_buffer_t;

typedef enum {
    VOXEL_BUFFER_FRONT,
    VOXEL_BUFFER_BACK
} VOXEL_BUFFER_T;

extern voxel_double_buffer_t* voxel_buffer;

static inline bool voxel_in_cylinder(int x, int y) {
    x = (x * 2) - (VOXELS_X - 1);
    y = (y * 2) - (VOXELS_Y - 1);
    return (x * x + y * y) <= (((VOXELS_X + VOXELS_Y) / 2) * ((VOXELS_X + VOXELS_Y) / 2));
}

bool voxel_buffer_map(void);
void voxel_buffer_unmap(void);

pixel_t* voxel_buffer_get(VOXEL_BUFFER_T buffer);
void voxel_buffer_clear(pixel_t* volume);
void voxel_buffer_swap(void);

#endif
