#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>

#include "array.h"
#include "mathc.h"
#include "rammel.h"
#include "input.h"
#include "graphics.h"
#include "model.h"
#include "voxel.h"

static float tess_vertices[16][VEC4_SIZE] = {
    { 1, 1, 1, 1}, { 1, 1, 1,-1}, { 1, 1,-1, 1}, { 1, 1,-1,-1},
    { 1,-1, 1, 1}, { 1,-1, 1,-1}, { 1,-1,-1, 1}, { 1,-1,-1,-1},
    {-1, 1, 1, 1}, {-1, 1, 1,-1}, {-1, 1,-1, 1}, {-1, 1,-1,-1},
    {-1,-1, 1, 1}, {-1,-1, 1,-1}, {-1,-1,-1, 1}, {-1,-1,-1,-1},
};

[[maybe_unused]] static int tess_faces[24][4] = {
    { 0,  1,  5,  4}, { 0,  2,  6,  4}, { 0,  8, 12,  4}, { 0,  2,  3,  1}, { 0,  1,  9,  8}, { 0,  2, 10,  8},
    { 1,  3,  7,  5}, { 1,  9, 13,  5}, { 1,  9, 11,  3}, { 2,  3,  7,  6}, {11, 10,  2,  3}, { 2, 10, 14,  6},
    { 3, 11, 15,  7}, { 4, 12, 13,  5}, { 4,  6, 14, 12}, { 4,  6,  7,  5}, { 5,  7, 15, 13}, { 7,  6, 14, 15},
    { 8, 10, 14, 12}, { 8,  9, 13, 12}, { 9,  8, 10, 11}, { 9, 11, 15, 13}, {10, 11, 15, 14}, {12, 14, 15, 13},
};

static int tess_edges[32][2] = {
    {0, 1}, {0, 2}, {0, 4}, {0, 8},
    {1, 3}, {1, 5}, {1, 9},
    {2, 3}, {2, 6}, {2, 10},
    {3, 7}, {3, 11},
    {4, 5}, {4, 6}, {4, 12},
    {5, 7}, {5, 13},
    {6, 7}, {6, 14},
    {7, 15},
    {8, 9}, {8, 10}, {8, 12},
    {9, 11}, {9, 13},
    {10, 11}, {10, 14},
    {11, 15},
    {12, 13}, {12, 14},
    {13, 15},
    {14, 15}
};

static const pixel_t colours[] = {
    RGBPIX(255,  0,  0),
    RGBPIX(255,255,  0),
    RGBPIX(  0,255,  0),
    RGBPIX(  0,255,255),
    RGBPIX(  0,  0,255),
};


static float *vec4_transform(float *result, float *v0, float *m0) {
	float x = v0[0];
	float y = v0[1];
	float z = v0[2];
	float w = v0[3];
    
	result[0] = m0[0] * x + m0[4] * y + m0[ 8] * z + m0[12] * w;
	result[1] = m0[1] * x + m0[5] * y + m0[ 9] * z + m0[13] * w;
	result[2] = m0[2] * x + m0[6] * y + m0[10] * z + m0[14] * w;
	result[3] = m0[3] * x + m0[7] * y + m0[11] * z + m0[15] * w;

	return result;
}

int main(int argc, char** argv) {

    if (!voxel_buffer_map()) {
        exit(1);
    }

    float volume_centre[VEC3_SIZE] = {VOXELS_X/2, VOXELS_Y/2, VOXELS_Z/2};
    float model_rotation[VEC3_SIZE] = {0, 0, 0};
    
    vec3_t transformed[count_of(tess_vertices)];
    mfloat_t matrix[MAT4_SIZE];

    float dist = 4;
    float fovt = dist*16;
    
    input_set_nonblocking();

    bool show_faces = false;

    for (int ch = 0; ch != 27; ch = getchar()) {
        pixel_t* volume = voxel_buffer_get(VOXEL_BUFFER_BACK);
        voxel_buffer_clear(volume);

        if (ch == 'f') {
            show_faces = !show_faces;
        }

        model_rotation[0] = fmodf(model_rotation[0] + 0.013f, 2 * M_PI);
        model_rotation[2] = fmodf(model_rotation[2] + 0.017f, 2 * M_PI);

        mat4_identity(matrix);
        mat4_apply_rotation(matrix, model_rotation);        

        for (uint i = 0; i < count_of(tess_vertices); ++i) {
            float vp[VEC4_SIZE];
            vec4_transform(vp, tess_vertices[i], matrix);

            float s = fovt / (vp[2] + dist);
            transformed[i].x =  vp[3] * s + volume_centre[0];
            transformed[i].y = -vp[1] * s + volume_centre[1];
            transformed[i].z = -vp[0] * s + volume_centre[2];
        }

        if (show_faces) {
            for (uint i = 0; i < count_of(tess_faces); ++i) {
                pixel_t colour = colours[i % count_of(colours)] & 0b01101101;
                graphics_draw_triangle(volume, transformed[tess_faces[i][0]].v, transformed[tess_faces[i][1]].v, transformed[tess_faces[i][2]].v, colour, NULL, NULL, NULL, NULL);
                graphics_draw_triangle(volume, transformed[tess_faces[i][0]].v, transformed[tess_faces[i][2]].v, transformed[tess_faces[i][3]].v, colour, NULL, NULL, NULL, NULL);
            }
        }

        for (uint i = 0; i < count_of(tess_edges); ++i) {
            pixel_t colour = colours[i % count_of(colours)];
            graphics_draw_line(volume, transformed[tess_edges[i][0]].v, transformed[tess_edges[i][1]].v, colour);
        }

        voxel_buffer_swap();
        usleep(50000);
    }

    voxel_buffer_unmap();

    return 0;
}




