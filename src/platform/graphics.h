#ifndef _GRAPHICS_H_
#define _GRAPHICS_H_

#include "voxel.h"

typedef uint32_t index_t;
struct image_s;

typedef struct {
    pixel_t colour;
    float texcoord[3][2];
    struct image_s* texture;
} triangle_state_t;

typedef void (*graphics_draw_voxel_cb_t)(pixel_t* volume, const int* coordinate, const float* barycentric, const triangle_state_t* triangle);
extern graphics_draw_voxel_cb_t graphics_triangle_shader_cb;

float* vec3_transform(float* vdst, const float* vsrc, const float* matrix);
float* mat4_apply_scale(float* matrix, const float* scale);
float* mat4_apply_scale_f(float* matrix, float scale);
float* mat4_apply_translation(float* matrix, const float* vector);
float* mat4_apply_rotation_x(float* matrix, float angle);
float* mat4_apply_rotation_y(float* matrix, float angle);
float* mat4_apply_rotation_z(float* matrix, float angle);
float* mat4_apply_rotation(float* matrix, const float* euler);

void graphics_draw_line(pixel_t* volume, const float* one, const float* two, pixel_t colour);
void graphics_triangle_colour(pixel_t colour);
void graphics_triangle_texture(const float* uv0, const float* uv1, const float* uv2, struct image_s* texture);
void graphics_draw_triangle(pixel_t* volume, const float* v0, const float* v1, const float* v2);

#endif
