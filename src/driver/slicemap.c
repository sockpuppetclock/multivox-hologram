#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "slicemap.h"

#include "mathc.h"
#include "rammel.h"
#include "rotation.h"
#include "gadget.h"

//#define SLICE_INFO
#define DESHIMMER_PERIMETER

voxel_2D_t slice_map[SLICE_COUNT][PANEL_WIDTH][PANEL_COUNT];

_Static_assert(((SLICE_COUNT)&3)==0, "slice count must be a multiple of 4");
_Static_assert((1<<(sizeof(((slice_polar_t*)0)->slice )*8)) >= SLICE_COUNT, "slice precision overflow");
_Static_assert((1<<(sizeof(((slice_polar_t*)0)->column)*8)) >= PANEL_WIDTH, "matrix width overflow");

#ifndef PANEL_0_ECCENTRICITY
#define ECCENTRICITY_0 0
#else
#define ECCENTRICITY_0 PANEL_0_ECCENTRICITY
#endif

#ifndef PANEL_1_ECCENTRICITY
#define ECCENTRICITY_1 0
#else
#define ECCENTRICITY_1 PANEL_1_ECCENTRICITY
#endif

float eccentricity[2] = {ECCENTRICITY_0, ECCENTRICITY_1}; // each panel's offset from the axis, in units of led pitch


// extended bit reversal
void slicemap_ebr(int* a, int n) {
    if (n == 1) {
        a[0] = 0;
        return;
    }

    int k = n / 2;

    #define S(i_) ((i_) < k ? (i_) : ((i_) + 1))

    slicemap_ebr(a, k);
    if (!(n & 1)) {
        for (int i = k - 1; i >= 1; i -= 1) {
            a[2 * i] = a[i];
        }
        for (int i = 1; i <= n - 1; i += 2) {
            a[i] = a[i - 1] + k;
        }
    } else {
        for (int i = k - 1; i >= 1; i -= 1) {
            a[S(2 * i)] = a[i];
        }
        for (int i = 1; i <= n - 2; i += 2) {
            a[S(i)] = a[S(i - 1)] + k + 1;
        }
        a[k] = k;
    }
}

void slicemap_init(slice_brightness_t brightness) {
    // build the lookup table mapping slices to voxels
    // each voxel should be visited exactly once by each side of each panel - four times in total
    // relaxing this rule allows a brighter image at the cost of uniformity across the volume

    int slice[SLICE_QUADRANT];
    slicemap_ebr(slice, count_of(slice));

    memset(slice_map, 0xff, sizeof(slice_map));

    uint8_t taken[VOXELS_Y][VOXELS_X][PANEL_COUNT][2] = {0};

    const vec2_t vox_centre = {.x=(float)(VOXELS_X-1) * 0.5f, .y=(float)(VOXELS_Y-1) * 0.5f};

    const float tolerancesq[] = {sqr(0.0625f), sqr(0.125f), sqr(0.25f), sqr(0.5f), sqr(0.7f)};

    const int passes = (brightness == SLICE_BRIGHTNESS_BOOSTED ? 2 : 1);
    for (int pass = 0; pass < passes; ++pass) {
        for (int tolerance = 0; tolerance < count_of(tolerancesq); ++tolerance) {
            int found = 0;
            for (int ia = 0; ia < SLICE_QUADRANT; ++ia) {
                int a = slice[ia];
                float angle = (float)a * M_PI * 2.0f / SLICE_COUNT;
                vec2_t slope = {.x = cosf(angle), .y = sinf(angle)};

                vec2_t eccoff[2]= {
                    {.x =  slope.y * eccentricity[0], .y = -slope.x * eccentricity[0]},
                    {.x = -slope.y * eccentricity[1], .y =  slope.x * eccentricity[1]}
                };

                for (int column = 0; column < PANEL_WIDTH; ++column) {
                    float coff = (float)column - ((float)(PANEL_WIDTH - 1) * 0.5f);
                    int side = coff > 0;

                    for (int panel = 0; panel < 2; ++panel) {
#ifdef DESHIMMER_PERIMETER
                        if ((panel == 0) && (column == 0 || column == PANEL_WIDTH-1)) {
                            // skipping the outer columns of panel 0 keeps it inside the same radius as
                            // panel 1, reducing shimmer around the edge
                            continue;
                        }
#endif
                        if (slice_map[a][column][panel].x >= VOXELS_X) {
                            vec2_t voxel_actual = {
                                .x = vox_centre.x + (eccoff[panel].x + slope.x * coff) * (1 - panel * 2),
                                .y = vox_centre.y + (eccoff[panel].y + slope.y * coff) * (1 - panel * 2),
                            };
                            vec2i_t voxel_virtual = {
                                .x = (int)roundf(voxel_actual.x),
                                .y = (int)roundf(voxel_actual.y),
                            };

                            float closest = FLT_MAX;
                            voxel_2D_t voxel;

                            for (int y = max(0, voxel_virtual.y - 1); y <= min(VOXELS_Y-1, voxel_virtual.y + 1); ++y) {
                                for (int x = max(0, voxel_virtual.x - 1); x <= min(VOXELS_X-1, voxel_virtual.x + 1); ++x) {
                                    if ((brightness == SLICE_BRIGHTNESS_UNLIMITED) || taken[y][x][panel][side] <= pass) {
                                        float distsq = vec2_distance_squared(voxel_actual.v, (float[]){x, y});
                                        if (distsq < closest) {
                                            closest = distsq;
                                            voxel.x = x;
                                            voxel.y = y;
                                        }
                                    }
                                }
                            }

                            if (closest <= tolerancesq[tolerance]) {
                                for (int q = 0; q < 4; ++q) {
                                    slice_map[a + q * SLICE_QUADRANT][column][panel] = voxel;
                                    taken[voxel.y][voxel.x][panel][side]++;

                                    int swap = voxel.x;
                                    voxel.x = VOXELS_X - 1 - voxel.y;
                                    voxel.y = swap;
                                }
                                ++found;
                            }
                        }
                    }
                }
            }

#ifdef SLICE_INFO
            int coverage = 0;
            int luminance = 0;

            for (int y = 0; y < VOXELS_Y; ++y) {
                for (int x = 0; x < VOXELS_X; ++x) {
                    coverage += (taken[y][x][0][0]!=0) + (taken[y][x][0][1]!=0) + (taken[y][x][1][0]!=0) + (taken[y][x][1][1]!=0);
                }
            }

            for (int a = 0; a < SLICE_COUNT; ++a) {
                for (int c = 0; c < PANEL_WIDTH; ++c) {
                    for (int p = 0; p < 2; ++p) {
                        luminance += (slice_map[a][c][p].x < VOXELS_X);
                    }
                }
            }

            printf("[%d %8d] coverage %2d%%, luminance %2d%%\n", tolerance, found, (coverage*127)/(VOXELS_X*VOXELS_Y*4), (luminance*100)/(SLICE_COUNT*PANEL_WIDTH*PANEL_COUNT));
#endif
        }
    }

#ifdef SLICE_INFO
    {
        char boxes[16][4] = {" ", "▗", "▖", "▄", "▝", "▐", "▞", "▟", "▘", "▚", "▌", "▙", "▀", "▜", "▛", "█"};
        char ansi_reset[] = "\x1b[30m";
        char ansi_black[] = "\x1b[0m";
        char ansi_grey170[] = "\x1b[37m";
        char ansi_grey85[] = "\x1b[90m";


        for (int y = 0; y < VOXELS_Y; ++y) {
            for (int x = 0; x < VOXELS_X; ++x) {
                uint b = (taken[y][x][0][0]!=0) | ((taken[y][x][0][1]!=0)<<1) | ((taken[y][x][1][0]!=0)<<2) | ((taken[y][x][1][1]!=0)<<3);
                if (taken[y][x][0][0]>1 || taken[y][x][0][1]>1 || taken[y][x][1][0]>1 || taken[y][x][1][1]>1) {
                    printf(ansi_black);
                } else {
                    if ((x^y)&1) {
                        printf(ansi_grey85);
                    } else {
                        printf(ansi_grey170);
                    }
                }
                printf("%s", boxes[b]);
            }
            
            printf("\n");
            printf(ansi_reset);
        }
    }
#endif
}
