#ifndef _GADGET_H_
#define _GADGET_H_

#define SPIN_SYNC 1

#define RGB_0_B1 12
#define RGB_0_G1 9
#define RGB_0_R1 6
#define RGB_0_B2 5
#define RGB_0_G2 8
#define RGB_0_R2 7

#define RGB_1_B1 21
#define RGB_1_G1 13
#define RGB_1_R1 20
#define RGB_1_B2 26
#define RGB_1_G2 19
#define RGB_1_R2 16

#define ADDR_CLK 4
#define ADDR_DAT 18
#define ADDR__EN 15
#define ADDR__EN_MASK (1<<ADDR__EN)

#define RGB_BLANK 11
#define RGB_CLOCK 23
#define RGB_STROBE 27
#define RGB_BLANK_MASK (1<<RGB_BLANK)
#define RGB_CLOCK_MASK (1<<RGB_CLOCK)
#define RGB_STROBE_MASK (1<<RGB_STROBE)

#define RGB_0_MASK ((1<<RGB_0_R1)|(1<<RGB_0_G1)|(1<<RGB_0_B1)|(1<<RGB_0_R2)|(1<<RGB_0_G2)|(1<<RGB_0_B2))
#define RGB_1_MASK ((1<<RGB_1_R1)|(1<<RGB_1_G1)|(1<<RGB_1_B1)|(1<<RGB_1_R2)|(1<<RGB_1_G2)|(1<<RGB_1_B2))
#define RGB_BITS_MASK (RGB_0_MASK | RGB_1_MASK)

static const int matrix_init_out[] = {RGB_0_B1, RGB_0_G1, RGB_0_R1, RGB_0_B2, RGB_0_G2, RGB_0_R2, RGB_1_B1, RGB_1_G1, RGB_1_R1, RGB_1_B2, RGB_1_G2, RGB_1_R2, ADDR_CLK, ADDR_DAT, ADDR__EN, RGB_BLANK, RGB_CLOCK, RGB_STROBE};

#define PANEL_WIDTH  128
#define PANEL_HEIGHT 64
#define PANEL_COUNT 2
#define PANEL_MULTIPLEX 2
#define PANEL_FIELD_HEIGHT (PANEL_HEIGHT / PANEL_MULTIPLEX)

#define PANEL_0_ORDER(c) (c)
#define PANEL_1_ORDER(c) (c)

#define PANEL_0_ECCENTRICITY 13.5
#define PANEL_1_ECCENTRICITY 0.375

#define VOXELS_X 128
#define VOXELS_Y 128
#define VOXELS_Z 64

#define VOXEL_Z_STRIDE 1
#define VOXEL_X_STRIDE VOXELS_Z
#define VOXEL_Y_STRIDE (VOXEL_X_STRIDE * VOXELS_X)
#define VOXELS_COUNT (VOXELS_X*VOXELS_Y*VOXELS_Z)

#define ROTATION_ZERO 286

#define CLOCK_WAITS 5

#endif
