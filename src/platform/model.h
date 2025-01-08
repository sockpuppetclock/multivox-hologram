#ifndef _MODEL_H_
#define _MODEL_H_

#include "mathc.h"
#include "voxel.h"
#include "graphics.h"

typedef struct {
    char* name;
    pixel_t colour;
    char* image;
} material_t;

typedef struct {
    vec3_t position;
    vec2_t texcoord;
    //vec3_t normal;
} vertex_t;

typedef struct {
    index_t index[2];
    pixel_t colour;
} edge_t;

typedef struct {
    uint32_t index_count;
    index_t* indices;
    pixel_t colour;
    struct image_s* image;
} surface_t;

typedef struct {
    uint32_t vertex_count;
    vertex_t* vertices;

    uint32_t edge_count;
    edge_t* edges;
    
    uint32_t surface_count;
    surface_t* surfaces;
} model_t;

typedef enum {
    STYLE_DEFAULT,
    STYLE_WIREFRAME_ALWAYS,
    STYLE_WIREFRAME_IF_UNDEFINED,
    STYLE_COUNT
} model_style_t;


model_t* model_load(const char* filename, model_style_t style);
model_t* model_load_image(const char* filename);
void model_set_colour(model_t* model, pixel_t colour);
void model_free(model_t* model);
void model_draw(pixel_t* volume, const model_t* model, float* matrix);
void model_get_bounds(model_t* model, vec3_t* centre, float* radius, float* height);


#endif
