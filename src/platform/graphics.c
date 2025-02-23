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

static triangle_state_t triangle_state;

static void draw_triangle_flat(pixel_t* volume, const int* coordinate, const float* barycentric, const triangle_state_t* triangle) {
    volume[VOXEL_INDEX(coordinate[0], coordinate[1], coordinate[2])] = triangle->colour;
}

static void draw_triangle_textured(pixel_t* volume, const int* coordinate, const float* barycentric, const triangle_state_t* triangle) {
    bool masked = false;

    float texcoord[2] = {
        triangle->texcoord[0][0] * barycentric[0] + triangle->texcoord[1][0] * barycentric[1] + triangle->texcoord[2][0] * barycentric[2],
        triangle->texcoord[0][1] * barycentric[0] + triangle->texcoord[1][1] * barycentric[1] + triangle->texcoord[2][1] * barycentric[2]
    };
    pixel_t colour = image_sample(triangle->texture, texcoord, &masked);
    
    if (!masked) {
        volume[VOXEL_INDEX(coordinate[0], coordinate[1], coordinate[2])] = colour;
    }
}


graphics_draw_voxel_cb_t graphics_triangle_shader_cb = NULL;
static graphics_draw_voxel_cb_t draw_voxel_cb = draw_triangle_flat;


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

float* vec3_transform(float* vdst, const float* vsrc, const float* matrix) {
    float vec[3] = {vsrc[0], vsrc[1], vsrc[2]};
    for (int i = 0; i < 3; ++i) {
        vdst[i] = matrix[i] * vec[0] + matrix[i+4] * vec[1] + matrix[i+8] * vec[2] + matrix[i+12];
    }
	return vdst;
}

float* mat4_apply_scale(float* matrix, const float* scale) {
    for (int i = 0; i < 12; ++i) {
        matrix[i] *= scale[i/4];
    }
    return matrix;
}

float* mat4_apply_scale_f(float* matrix, float scale) {
    for (int i = 0; i < 12; ++i) {
        matrix[i] *= scale;
    }
    return matrix;
}

float* mat4_apply_translation(float* matrix, const float* vector) {
    for (int i = 0; i < 4; ++i) {
        matrix[i+12] += matrix[i] * vector[0] + matrix[i+4] * vector[1] + matrix[i+8] * vector[2];
    }
    return matrix;
}

float *mat4_apply_rotation_x(float *result, float angle) {
    float multiplied[8];

    float c = cosf(angle);
    float s = sinf(angle);

    multiplied[0] = result[4] * c + result[8] * s;
    multiplied[1] = result[5] * c + result[9] * s;
    multiplied[2] = result[6] * c + result[10]* s;
    multiplied[3] = result[7] * c + result[11]* s;
    multiplied[4] = result[4] *-s + result[8] * c;
    multiplied[5] = result[5] *-s + result[9] * c;
    multiplied[6] = result[6] *-s + result[10]* c;
    multiplied[7] = result[7] *-s + result[11]* c;

    memcpy(&result[4], multiplied, sizeof(multiplied));
    return result;
}

float *mat4_apply_rotation_y(float *result, float angle) {
    float multiplied[8];

    float c = cosf(angle);
    float s = sinf(angle);

    multiplied[0] = result[0] * c + result[8] * -s;
    multiplied[1] = result[1] * c + result[9] * -s;
    multiplied[2] = result[2] * c + result[10]* -s;
    multiplied[3] = result[3] * c + result[11]* -s;
    multiplied[4] = result[0] * s + result[8]  * c;
    multiplied[5] = result[1] * s + result[9]  * c;
    multiplied[6] = result[2] * s + result[10] * c;
    multiplied[7] = result[3] * s + result[11] * c;

    memcpy(result, multiplied, sizeof(*multiplied)*4);
    memcpy(&result[8], &multiplied[4], sizeof(*multiplied)*4);
    return result;
}

float *mat4_apply_rotation_z(float *result, float angle) {
    float multiplied[8];

    float c = cosf(angle);
    float s = sinf(angle);
    
    multiplied[0] = result[0] *  c + result[4] * s;
    multiplied[1] = result[1] *  c + result[5] * s;
    multiplied[2] = result[2] *  c + result[6] * s;
    multiplied[3] = result[3] *  c + result[7] * s;
    multiplied[4] = result[0] * -s + result[4] * c;
    multiplied[5] = result[1] * -s + result[5] * c;
    multiplied[6] = result[2] * -s + result[6] * c;
    multiplied[7] = result[3] * -s + result[7] * c;

    memcpy(result, multiplied, sizeof(multiplied));
    return result;
}

float* mat4_apply_rotation(float* matrix, const float* euler) {
    float cp = cosf(euler[0]);
    float sp = sinf(euler[0]);
    float cr = cosf(euler[1]);
    float sr = sinf(euler[1]);
    float cy = cosf(euler[2]);
    float sy = sinf(euler[2]);

    float rotation[MAT4_SIZE] = {
        cy*cr-sy*sp*sr, cr*sy+cy*sp*sr,-cp*sr, 0,
       -cp*sy         , cy*cp         , sp   , 0,
        cy*sr+cr*sy*sp, sy*sr-cy*cr*sp, cp*cr, 0,
        0             , 0             , 0    , 1,
    };

    return mat4_multiply(matrix, matrix, rotation);
}


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
        while (ey > 0) {
            y += sdy;
            --ey;
        }
        while (ez > 0) {
            z += sdz;
            --ez;
        }
        
        uint idx = (int)x * x_stride + (int)y * y_stride + (int)z * z_stride;
        if (idx < VOXELS_COUNT) {
            volume[idx] = colour;
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

    int c[3];
    sort_channels(delta, &c[0], &c[1], &c[2]);

    const uint stride[3] = {VOXEL_X_STRIDE, VOXEL_Y_STRIDE, VOXEL_Z_STRIDE};
    if (one.v[c[2]] < two.v[c[2]]) {
        draw_line_(volume, one.v[c[2]], two.v[c[2]], one.v[c[1]], two.v[c[1]], one.v[c[0]], two.v[c[0]], stride[c[2]], stride[c[1]], stride[c[0]], colour);
    } else {
        draw_line_(volume, two.v[c[2]], one.v[c[2]], two.v[c[1]], one.v[c[1]], two.v[c[0]], one.v[c[0]], stride[c[2]], stride[c[1]], stride[c[0]], colour);
    }
}

#ifdef TINY_TRIANGLES_CENTRED
static void draw_tiny_triangle(pixel_t* volume, const float* v0, const float* v1, const float* v2, pixel_t colour) {

    int pos[VEC3_SIZE] = {
        (int)roundf((v0[0] + v1[0] + v2[0]) * (1.0f/3.0f)),
        (int)roundf((v0[1] + v1[1] + v2[1]) * (1.0f/3.0f)),
        (int)roundf((v0[2] + v1[2] + v2[2]) * (1.0f/3.0f)),
    };

    if ((uint)pos[0] >= VOXELS_X || (uint)pos[1] >= VOXELS_Y || (uint)pos[2] >= VOXELS_Z) {
        return;
    }

    float bary[3] = {1.0f/3.0f, 1.0f/3.0f, 1.0f/3.0f};

    draww_voxel_cb(volume, pos, bary, &triangle_state);
}
#else
static void draw_tiny_triangle(pixel_t* volume, const float* v0, const float* v1, const float* v2) {

    int pos[VEC3_SIZE] = {(int)v0[0], (int)v0[1], (int)v0[2]};
    if ((uint)pos[0] >= VOXELS_X || (uint)pos[1] >= VOXELS_Y || (uint)pos[2] >= VOXELS_Z) {
        return;
    }

    float bary[3] = {1.0f, 0.0f, 0.0f};

    draw_voxel_cb(volume, pos, bary, &triangle_state);
}
#endif

void graphics_triangle_colour(pixel_t colour) {
    triangle_state.colour = colour;
    triangle_state.texture = NULL;
}

void graphics_triangle_texture(const float* uv0, const float* uv1, const float* uv2, image_t* texture) {
    vec3_assign(triangle_state.texcoord[0], uv0);
    vec3_assign(triangle_state.texcoord[1], uv1);
    vec3_assign(triangle_state.texcoord[2], uv2);
    triangle_state.texture = texture;
}

void graphics_draw_triangle(pixel_t* volume, const float* v0, const float* v1, const float* v2) {
    // voxelise a triangle by rendering it on its most flat axis

    if (graphics_triangle_shader_cb) {
        draw_voxel_cb = graphics_triangle_shader_cb;
    } else {
        if (triangle_state.texture) {
            draw_voxel_cb = draw_triangle_textured;
        } else {
            draw_voxel_cb = draw_triangle_flat;
        }
    }

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
        draw_tiny_triangle(volume, v0, v1, v2);
        return;
    }

    vec3_abs(minor, minor);

    int xchannel, ychannel, zchannel;
    sort_channels(minor, &xchannel, &ychannel, &zchannel);
    if (ab_max[xchannel] - ab_min[xchannel] < ab_max[ychannel] - ab_min[ychannel]) {
        // favour wide spans for early-out
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
                    draw_voxel_cb(volume, voxel, w, &triangle_state);
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


