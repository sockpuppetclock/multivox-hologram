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
#include <sched.h>
#include <time.h>

#include "graphics.h"
#include "mathc.h"
#include "rammel.h"
#include "array.h"
#include "image.h"

//#define CHECK_BOUNDS

graphics_draw_voxel_cb_t graphics_draw_voxel_cb = NULL;

#ifdef TRIANGLE_DITHER
//some content looks better if we break up the stairstepping on shallow triangles
float graphics_triangle_fuzz = 0.25f;
#endif

static inline bool clip(float* one, float* two, const uint c, const float maxval) {
    if (one[c] > two[c]) {
        float* t = two;
        two = one;
        one = t;
    }

    if (two[c] < 0 || one[c] > maxval) {
        return true;
    }

    if (one[c] < 0) {
        float delta[VEC3_SIZE];
        vec3_subtract(delta, two, one);
        vec3_multiply_f(delta, delta, -one[c] / delta[c]);
        vec3_add(one, one, delta);
    }

    if (two[c] > maxval) {
        float delta[VEC3_SIZE];
        vec3_subtract(delta, two, one);
        vec3_multiply_f(delta, delta, (two[c] - maxval) / delta[c]);
        vec3_subtract(two, two, delta);
    }

    return false;
}

float *vec3_transform(float *result, const float *v0, const float *m0) {
	float x = v0[0];
	float y = v0[1];
	float z = v0[2];
	result[0] = m0[0] * x + m0[4] * y + m0[8] * z + m0[12];
	result[1] = m0[1] * x + m0[5] * y + m0[9] * z + m0[13];
	result[2] = m0[2] * x + m0[6] * y + m0[10] * z + m0[14];
	return result;
}

float *mat4_apply_scale(float *result, float s) {
    float temp[MAT4_SIZE] = {s,0,0,0, 0,s,0,0, 0,0,s,0, 0,0,0,1};
    mat4_multiply(result, result, temp);
    return result;
}

float *mat4_apply_translation(float *result, const float *v0) {
    float temp[MAT4_SIZE] = {1,0,0,0, 0,1,0,0, 0,0,1,0, v0[0],v0[1],v0[2],1};
    mat4_multiply(result, result, temp);
    return result;
}

float *mat4_apply_rotation_x(float *result, float angle) {
    float temp[MAT4_SIZE];
    
    mat4_identity(temp);
    mat4_rotation_x(temp, angle);
    mat4_multiply(result, result, temp);

    return result;
}

float *mat4_apply_rotation_y(float *result, float angle) {
    float temp[MAT4_SIZE];
    
    mat4_identity(temp);
    mat4_rotation_y(temp, angle);
    mat4_multiply(result, result, temp);

    return result;
}

float *mat4_apply_rotation_z(float *result, float angle) {
    float temp[MAT4_SIZE];
    
    mat4_identity(temp);
    mat4_rotation_z(temp, angle);
    mat4_multiply(result, result, temp);

    return result;
}

float *mat4_apply_rotation(float *result, const float *euler) {
    float temp[MAT4_SIZE];
    
    mat4_identity(temp);
    mat4_rotation_z(temp, euler[2]);
    mat4_multiply(result, result, temp);
    
    mat4_identity(temp);
    mat4_rotation_x(temp, euler[0]);
    mat4_multiply(result, result, temp);
    
    mat4_identity(temp);
    mat4_rotation_y(temp, euler[1]);
    mat4_multiply(result, result, temp);

    return result;
}

void graphics_fade_buffer(pixel_t* dst, pixel_t* src) {
    for (uint i = 0; i < VOXELS_COUNT; ++i) {
        uint8_t pix = *src++;
        pix = (pix & 0b11011010) >> 1;
        //pix -= (((pix & 0b10010000)>>2) | ((pix & 0b01001010)>>1) | (pix & 0b00100101));
        //pix = (pix & 0b11011011) - (((pix & 0b10010010)>>1) | (pix & 0b01001001));
        *dst++ = pix;
    }
}

static void draw_line_(pixel_t* volume, float x0, float x1, float y0, float y1, float z0, float z1, uint x_stride, uint y_stride, uint z_stride, pixel_t colour) {
    float x = x0;
    float y = y0;
    float z = z0;
    float dx = x1 - x0;
    float dy = y1 - y0;
    float dz = z1 - z0;
    float edy = fabsf(dy / dx);
    float edz = fabsf(dz / dx);
    float sdy, sdz;

    float ey = (0.5f - fmodf(x0, 1.0f)) * edy + fmodf(y0, 1.0f);
    if (dy >= 0) {
        sdy = 1.0f;
        ey -= 1.0f;
    } else {
        sdy = -1.0f;
        ey = -ey;
    }
    
    float ez = (0.5f - fmodf(x0, 1.0f)) * edz + fmodf(z0, 1.0f);
    if (dz >= 0) {
        sdz = 1.0f;
        ez -= 1.0f;
    } else {
        sdz = -1.0f;
        ez = -ez;
    }

    while (x < x1) {
        uint idx = (int)x * x_stride + (int)y * y_stride + (int)z * z_stride;
        if (idx < VOXELS_COUNT) {
            volume[idx] = colour;
        }
        while (ey > 0) {
            y += sdy;
            --ey;
        }
        while (ez > 0) {
            z += sdz;
            --ez;
        }
        
        ++x;
        ey += edy;
        ez += edz;
    }
}

void graphics_draw_line(pixel_t* volume, const float* pone, const float* ptwo, pixel_t colour) {
    vec3_t one = {.x=pone[0], .y=pone[1], .z=pone[2]};
    vec3_t two = {.x=ptwo[0], .y=ptwo[1], .z=ptwo[2]};

    if (clip(one.v, two.v, 0, VOXELS_X-1)) {
        return;
    }
    if (clip(one.v, two.v, 1, VOXELS_Y-1)) {
        return;
    }
    if (clip(one.v, two.v, 2, VOXELS_Z-1)) {
        return;
    }

    float delta[VEC3_SIZE];
    vec3_subtract(delta, one.v, two.v);
    vec3_abs(delta, delta);

    if (delta[0] > delta[1] && delta[0] > delta[2]) {
        if (one.x < two.x) {
            draw_line_(volume, one.x, two.x, one.y, two.y, one.z, two.z, VOXEL_X_STRIDE, VOXEL_Y_STRIDE, VOXEL_Z_STRIDE, colour);
        } else {
            draw_line_(volume, two.x, one.x, two.y, one.y, two.z, one.z, VOXEL_X_STRIDE, VOXEL_Y_STRIDE, VOXEL_Z_STRIDE, colour);
        }
    } else if (delta[1] > delta[0] && delta[1] > delta[2]) {
        if (one.y < two.y) {
            draw_line_(volume, one.y, two.y, one.x, two.x, one.z, two.z, VOXEL_Y_STRIDE, VOXEL_X_STRIDE, VOXEL_Z_STRIDE, colour);
        } else {
            draw_line_(volume, two.y, one.y, two.x, one.x, two.z, one.z, VOXEL_Y_STRIDE, VOXEL_X_STRIDE, VOXEL_Z_STRIDE, colour);
        }
    } else {
        if (one.z < two.z) {
            draw_line_(volume, one.z, two.z, one.y, two.y, one.x, two.x, VOXEL_Z_STRIDE, VOXEL_Y_STRIDE, VOXEL_X_STRIDE, colour);
        } else {
            draw_line_(volume, two.z, one.z, two.y, one.y, two.x, one.x, VOXEL_Z_STRIDE, VOXEL_Y_STRIDE, VOXEL_X_STRIDE, colour);
        }
    }
}

#ifdef TINY_TRIANGLES_CENTRED
static void draw_tiny_triangle(pixel_t* volume, const float* v0, const float* v1, const float* v2, pixel_t colour,
                                                const float* uv0, const float* uv1, const float* uv2, image_t* texture) {

    int pos[VEC3_SIZE] = {
        (int)roundf((v0[0] + v1[0] + v2[0]) * (1.0f/3.0f)),
        (int)roundf((v0[1] + v1[1] + v2[1]) * (1.0f/3.0f)),
        (int)roundf((v0[2] + v1[2] + v2[2]) * (1.0f/3.0f)),
    };

    if ((uint)pos[0] >= VOXELS_X || (uint)pos[1] >= VOXELS_Y || (uint)pos[2] >= VOXELS_Z) {
        return;
    }

    if (texture) {
        float uv[VEC2_SIZE] = {
            (uv0[0] + uv1[0] + uv2[0]) * (1.0f/3.0f),
            (uv0[1] + uv1[1] + uv2[1]) * (1.0f/3.0f),
        };
        bool masked;
        colour = image_sample(texture, uv, &masked);
        if (masked) {
            return;
        }
    }

    volume[VOXEL_INDEX(pos[0], pos[1], pos[2])] = colour;
}
#else
static void draw_tiny_triangle(pixel_t* volume, const float* v0, const float* v1, const float* v2, pixel_t colour,
                                                const float* uv0, const float* uv1, const float* uv2, image_t* texture) {

    int pos[VEC3_SIZE] = {(int)v0[0], (int)v0[1], (int)v0[2]};
    if ((uint)pos[0] >= VOXELS_X || (uint)pos[1] >= VOXELS_Y || (uint)pos[2] >= VOXELS_Z) {
        return;
    }

    if (texture) {
        bool masked;
        colour = image_sample(texture, uv0, &masked);
        if (masked) {
            return;
        }
    }

    volume[VOXEL_INDEX(pos[0], pos[1], pos[2])] = colour;
}
#endif


static void swap(int* a, int* b) {
    int t = *a;
    *a = *b;
    *b = t;
}

static void sort_channels(float* axis, int* xchannel, int* ychannel, int* zchannel) {
    *xchannel = 0;
    *ychannel = 1;
    *zchannel = 2;

    if (axis[*ychannel] > axis[*zchannel]) {
        swap(ychannel, zchannel);
    }
    if (axis[*xchannel] > axis[*zchannel]) {
        swap(xchannel, zchannel);
    }

    /* don't actually care about the order of x & y
    if (axis[*xchannel] > axis[*ychannel]) {
        swap(xchannel, ychannel);
    }*/ 
}

void graphics_draw_triangle(pixel_t* volume, const float* v0, const float* v1, const float* v2, pixel_t colour,
                                             const float* uv0, const float* uv1, const float* uv2, image_t* texture) {

    // voxelise a triangle by rendering it on its most flat axis

    float ab_min[VEC3_SIZE];
    vec3_min(ab_min, v0, v1);
    vec3_min(ab_min, ab_min, v2);
    if (ab_min[0] >= VOXELS_X || ab_min[1] >= VOXELS_Y || ab_min[2] >= VOXELS_Z) {
        return;
    }

    float ab_max[VEC3_SIZE];
    vec3_max(ab_max, v0, v1);
    vec3_max(ab_max, ab_max, v2);
    if (ab_max[0] < 0 || ab_max[1] < 0 || ab_max[2] < 0) {
        return;
    }

    float t1[VEC3_SIZE] = {v1[0]-v0[0], v1[1]-v0[1], v1[2]-v0[2]};
    float t2[VEC3_SIZE] = {v2[0]-v0[0], v2[1]-v0[1], v2[2]-v0[2]};
    float minor[VEC3_SIZE];
    vec3_cross(minor, t1, t2);

    float areasq2 = vec3_dot(minor, minor);
    if (areasq2 < 4) {
        draw_tiny_triangle(volume, v0, v1, v2, colour, uv0, uv1, uv2, texture);
        return;
    }

    vec3_abs(minor, minor);

    int xchannel, ychannel, zchannel;
    sort_channels(minor, &xchannel, &ychannel, &zchannel);
    if (ab_max[xchannel] - ab_min[xchannel] < ab_max[ychannel] - ab_min[ychannel]) {
        swap(&xchannel, &ychannel);
    }

    float vox_min[VEC3_SIZE] = {0, 0, 0};
    float vox_off[VEC3_SIZE] = {0.5f, 0.5f, 0.5f};
    float vox_max[VEC3_SIZE] = {VOXELS_X-1, VOXELS_Y-1, VOXELS_Z-1};
    vec3_max(ab_min, ab_min, vox_min);
    vec3_floor(ab_min, ab_min);
    vec3_add(ab_min, ab_min, vox_off);
    vec3_ceil(ab_max, ab_max);
    vec3_min(ab_max, ab_max, vox_max);
    vec3_add(ab_max, ab_max, vox_off);

    #define ORIENT2D(a, b, c) (((b)[xchannel] - (a)[xchannel]) * ((c)[ychannel] - (a)[ychannel]) - ((b)[ychannel] - (a)[ychannel]) * ((c)[xchannel] - (a)[xchannel]))

    float dx[VEC3_SIZE] = {v1[ychannel] - v2[ychannel], v2[ychannel] - v0[ychannel], v0[ychannel] - v1[ychannel]};
    float dy[VEC3_SIZE] = {v2[xchannel] - v1[xchannel], v0[xchannel] - v2[xchannel], v1[xchannel] - v0[xchannel]};

    float w0[VEC3_SIZE] = {
        ORIENT2D(v1, v2, ab_min),
        ORIENT2D(v2, v0, ab_min),
        ORIENT2D(v0, v1, ab_min)
    };

    float rden = 1.0f / ORIENT2D(v0, v1, v2);
    vec3_multiply_f(dx, dx, rden);
    vec3_multiply_f(dy, dy, rden);
    vec3_multiply_f(w0, w0, rden);

    #undef ORIENT2D

    int xrange = (int)(ab_max[xchannel] - ab_min[xchannel] + 1.5f);
    int yrange = (int)(ab_max[ychannel] - ab_min[ychannel] + 1.5f);
    const uint voxels_z = (uint[3]){VOXELS_X, VOXELS_Y, VOXELS_Z}[zchannel];

    int voxel[VEC3_SIZE];
    voxel[ychannel] = (int)floorf(ab_min[ychannel]);

    for (int y = 0; y < yrange; ++y) {
        float w[VEC3_SIZE] = {w0[0], w0[1], w0[2]};

        bool done = false;
        voxel[xchannel] = (int)floorf(ab_min[xchannel]);
        for (int x = 0; x < xrange; ++x) {
            const float e = -1e-5f;
            if (w[0] >= e && w[1] >= e && w[2] >= e) {
                done = true;
                
#ifdef TRIANGLE_DITHER
                float dither = ((float)(((x&1)<<1)|(y&1)) - 1.5f) * graphics_triangle_fuzz;
#else
                const float dither = 0;
#endif
                voxel[zchannel] = (int)floorf(v0[zchannel] * w[0] + v1[zchannel] * w[1] + v2[zchannel] * w[2] + dither);
                if ((uint)voxel[zchannel] < voxels_z) {
#ifdef CHECK_BOUNDS
                    if ((uint)voxel[0] >= VOXELS_X || (uint)voxel[1] >= VOXELS_Y || (uint)voxel[2] >= VOXELS_Z) {
                        printf("bwart\n");
                        voxel[0] &= (VOXELS_X-1);
                        voxel[1] &= (VOXELS_Y-1);
                        voxel[2] &= (VOXELS_Z-1);
                    }
#endif
                    bool masked = false;
                    if (texture) {
                        float texcoord[2] = {
                            uv0[0] * w[0] + uv1[0] * w[1] + uv2[0] * w[2],
                            uv0[1] * w[0] + uv1[1] * w[1] + uv2[1] * w[2]
                        };
                        colour = image_sample(texture, texcoord, &masked);
                    }
                    
                    if (!masked) {
                        if (graphics_draw_voxel_cb) {
                            graphics_draw_voxel_cb(volume, voxel, colour);
                        } else {
                            volume[VOXEL_INDEX(voxel[0], voxel[1], voxel[2])] = colour;
                        }
                    }
                }
            } else if (done) {
                break;
            }

            vec3_add(w, w, dx);
            voxel[xchannel] += 1.0f;
        }
        vec3_add(w0, w0, dy);
        voxel[ychannel] += 1.0f;
    }
}


void graphics_cleanup() {
}





