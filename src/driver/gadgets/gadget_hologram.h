#ifndef _GADGET_H_
#define _GADGET_H_

// generic HUB75 64x64

#define SPIN_SYNC 3

#define RGB_0_B1 7
#define RGB_0_G1 27
#define RGB_0_R1 11
#define RGB_0_B2 10
#define RGB_0_G2 9
#define RGB_0_R2 8

#define RGB_1_B1 6
#define RGB_1_G1 5
#define RGB_1_R1 12
#define RGB_1_B2 20
#define RGB_1_G2 13
#define RGB_1_R2 19

#define ROW_A 22
#define ROW_B 23
#define ROW_C 24
#define ROW_D 25
#define ROW_E 1
#define ROW_MASK ((1<<ROW_A)|(1<<ROW_B)|(1<<ROW_C)|(1<<ROW_D)|(1<<ROW_E))

#define RGB_BLANK 18 // OE
#define RGB_CLOCK 17
#define RGB_STROBE 4
#define RGB_BLANK_MASK (1<<RGB_BLANK)
#define RGB_CLOCK_MASK (1<<RGB_CLOCK)
#define RGB_STROBE_MASK (1<<RGB_STROBE)

#define RGB_0_MASK ((1<<RGB_0_R1)|(1<<RGB_0_G1)|(1<<RGB_0_B1)|(1<<RGB_0_R2)|(1<<RGB_0_G2)|(1<<RGB_0_B2))
#define RGB_1_MASK ((1<<RGB_1_R1)|(1<<RGB_1_G1)|(1<<RGB_1_B1)|(1<<RGB_1_R2)|(1<<RGB_1_G2)|(1<<RGB_1_B2))
#define RGB_BITS_MASK (RGB_0_MASK | RGB_1_MASK)

static const int matrix_init_out[] = {RGB_0_B1, RGB_0_G1, RGB_0_R1, RGB_0_B2, RGB_0_G2, RGB_0_R2, RGB_1_B1, RGB_1_G1, RGB_1_R1, RGB_1_B2, RGB_1_G2, RGB_1_R2, ROW_A, ROW_B, ROW_C, ROW_D, ROW_E, RGB_BLANK, RGB_CLOCK, RGB_STROBE};

#define PANEL_WIDTH  64
#define PANEL_HEIGHT 64
#define PANEL_COUNT 2
#define PANEL_MULTIPLEX 1
#define PANEL_FIELD_HEIGHT (PANEL_HEIGHT / PANEL_MULTIPLEX)

#define PANEL_0_ORDER(c) (c)
#define PANEL_1_ORDER(c) (c)

#define PANEL_0_ECCENTRICITY 13.5
#define PANEL_1_ECCENTRICITY 0.375

#define VOXELS_X 64
#define VOXELS_Y 64
#define VOXELS_Z 64

// xyz -> 1D array steps
#define VOXEL_Z_STRIDE 1
#define VOXEL_X_STRIDE VOXELS_Z
#define VOXEL_Y_STRIDE (VOXEL_X_STRIDE * VOXELS_X)
#define VOXELS_COUNT (VOXELS_X*VOXELS_Y*VOXELS_Z)

#define ROTATION_ZERO 286

#define CLOCK_WAITS 5

#endif
