#ifndef _GADGETCOMMON_H_
#define _GADGETCOMMON_H_

// all the gadget-specific stuff goes in gadget_gadgetname.h, and selected via `cmake -DMULTIVOX_GADGET=gadgetname ..`

#include <stdint.h>

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

typedef struct {
    pixel_t volume[2][VOXELS_COUNT];
    uint8_t page;
    uint8_t bpc;
    uint8_t rpds; //revolutions per decasecond
    uint8_t fpcs; //frames per centisecond
} volume_double_buffer_t;

#endif
