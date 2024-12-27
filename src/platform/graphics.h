#ifndef _GRAPHICS_H_
#define _GRAPHICS_H_

#include "mathc.h"
#include "gadget.h"
#include "image.h"

typedef uint32_t index_t;

float* vec3_transform(float* result, const float* v0, const float* m0);
float* mat4_apply_scale(float* result, float s);
float* mat4_apply_translation(float* result, const float* v0);
float* mat4_apply_rotation(float* result, const float* euler);

typedef void (*graphics_draw_voxel_cb_t)(pixel_t* volume, const int* coordinate, pixel_t colour);
extern graphics_draw_voxel_cb_t graphics_draw_voxel_cb;

void graphics_fade_buffer(pixel_t* dst, pixel_t* src);
void graphics_draw_line(pixel_t* volume, const float* one, const float* two, pixel_t colour);
void graphics_draw_triangle(pixel_t* volume, const float* v0, const float* v1, const float* v2, pixel_t colour, const float* t0, const float* t1, const float* t2, image_t* texture);
void graphics_cleanup();

#endif
