#pragma GCC push_options
//#pragma GCC optimize ("Og")

#include "objects.h"

#include <stdlib.h>

#include "mathc.h"
#include "rammel.h"
#include "gadget.h"
#include "graphics.h"
#include "model.h"
#include "zander.h"
#include "particles.h"
#include "terrain.h"


#define ZFLOAT(f) ((float)((double)((int32_t)f) / (double)(0x01000000)))

static const model_t objectSmallLeafyTree = {
    .vertices = (vertex_t*)(float[][5]){
        {ZFLOAT(0x00300000), ZFLOAT(0x00300000), -ZFLOAT(0xFE800000)},
        {ZFLOAT(0xFFD9999A), ZFLOAT(0x00000000), -ZFLOAT(0x00000000)},
        {ZFLOAT(0x00266666), ZFLOAT(0x00000000), -ZFLOAT(0x00000000)},
        {ZFLOAT(0x00000000), ZFLOAT(0xFF400000), -ZFLOAT(0xFEF33334)},
        {ZFLOAT(0x00800000), ZFLOAT(0xFF800000), -ZFLOAT(0xFF400000)},
        {ZFLOAT(0xFF400000), ZFLOAT(0xFFD55556), -ZFLOAT(0xFECCCCCD)},
        {ZFLOAT(0xFF800000), ZFLOAT(0x00400000), -ZFLOAT(0xFEA66667)},
        {ZFLOAT(0x00800000), ZFLOAT(0x002AAAAA), -ZFLOAT(0xFE59999A)},
        {ZFLOAT(0x00C00000), ZFLOAT(0xFFC00000), -ZFLOAT(0xFEA66667)},
        {ZFLOAT(0xFFA00000), ZFLOAT(0x00999999), -ZFLOAT(0xFECCCCCD)},
        {ZFLOAT(0x00C00000), ZFLOAT(0x00C00000), -ZFLOAT(0xFF400000)}
    },
    .vertex_count = 11,
    .surfaces = (surface_t[]){
        {3, (index_t[]){0, 9, 10}, RGBPIX(0,192,0)},
        {3, (index_t[]){0, 1, 2}, RGBPIX(192,0,0)},
        {9, (index_t[]){0, 3, 4, 0, 5, 6, 0, 7, 8}, RGBPIX(0,255,0)}
    },
    .surface_count = 3
};

static const model_t objectTallLeafyTree = {
    .vertices = (vertex_t*)(float[][5]){
        {ZFLOAT(0x0036DB6D), ZFLOAT(0x00300000), -ZFLOAT(0xFD733334)},
        {ZFLOAT(0xFFD00000), ZFLOAT(0x00000000), -ZFLOAT(0x00000000)},
        {ZFLOAT(0x00300000), ZFLOAT(0x00000000), -ZFLOAT(0x00000000)},
        {ZFLOAT(0x00000000), ZFLOAT(0xFF400000), -ZFLOAT(0xFE0CCCCD)},
        {ZFLOAT(0x00800000), ZFLOAT(0xFF800000), -ZFLOAT(0xFE59999A)},
        {ZFLOAT(0xFF533334), ZFLOAT(0xFFC92493), -ZFLOAT(0xFE333334)},
        {ZFLOAT(0xFF400000), ZFLOAT(0x00600000), -ZFLOAT(0xFEA66667)},
        {ZFLOAT(0x00000000), ZFLOAT(0xFF666667), -ZFLOAT(0xFF19999A)},
        {ZFLOAT(0xFF800000), ZFLOAT(0xFFA00000), -ZFLOAT(0xFF400000)},
        {ZFLOAT(0xFFA00000), ZFLOAT(0x00999999), -ZFLOAT(0xFE800000)},
        {ZFLOAT(0x00C00000), ZFLOAT(0x00C00000), -ZFLOAT(0xFECCCCCD)},
        {ZFLOAT(0xFFB33334), ZFLOAT(0x00E66666), -ZFLOAT(0xFF19999A)},
        {ZFLOAT(0x00800000), ZFLOAT(0x00C00000), -ZFLOAT(0xFF400000)},
        {ZFLOAT(0x00300000), ZFLOAT(0x00300000), -ZFLOAT(0xFE59999A)}
    },
    .vertex_count = 14,
    .surfaces = (surface_t[]){
        {3, (index_t[]){0, 1, 2}, RGBPIX(192,0,0)},
        {9, (index_t[]){0, 9, 10, 0, 5, 6, 13, 7, 8}, RGBPIX(0,192,0)},
        {6, (index_t[]){13, 11, 12, 0, 3, 4}, RGBPIX(0,255,0)}
    },
    .surface_count = 3
};

static const model_t objectGazebo = {
    .vertices = (vertex_t*)(float[][5]){
        {ZFLOAT(0x00000000), ZFLOAT(0x00000000), -ZFLOAT(0xFF000000)},
        {ZFLOAT(0xFF800000), ZFLOAT(0x00800000), -ZFLOAT(0xFF400000)},
        {ZFLOAT(0xFF800000), ZFLOAT(0xFF800000), -ZFLOAT(0xFF400000)},
        {ZFLOAT(0x00800000), ZFLOAT(0xFF800000), -ZFLOAT(0xFF400000)},
        {ZFLOAT(0x00800000), ZFLOAT(0x00800000), -ZFLOAT(0xFF400000)},
        {ZFLOAT(0xFF800000), ZFLOAT(0x00800000), -ZFLOAT(0x01000000)},
        {ZFLOAT(0xFF800000), ZFLOAT(0xFF800000), -ZFLOAT(0x01000000)},
        {ZFLOAT(0x00800000), ZFLOAT(0xFF800000), -ZFLOAT(0x01000000)},
        {ZFLOAT(0x00800000), ZFLOAT(0x00800000), -ZFLOAT(0x01000000)},
        {ZFLOAT(0xFF99999A), ZFLOAT(0x00800000), -ZFLOAT(0xFF400000)},
        {ZFLOAT(0xFF99999A), ZFLOAT(0xFF800000), -ZFLOAT(0xFF400000)},
        {ZFLOAT(0x00666666), ZFLOAT(0xFF800000), -ZFLOAT(0xFF400000)},
        {ZFLOAT(0x00666666), ZFLOAT(0x00800000), -ZFLOAT(0xFF400000)}
    },
    .vertex_count = 13,
    .surfaces = (surface_t[]){
        {12, (index_t[]){1, 5, 9, 2, 6, 10, 3, 7, 11, 4, 8, 12}, RGBPIX(192,192,192)},
        {6, (index_t[]){0, 1, 2, 0, 3, 4}, RGBPIX(96,96,0)},
        {6, (index_t[]){0, 1, 4, 0, 2, 3}, RGBPIX(255,0,0)}
    },
    .surface_count = 3
};

static const model_t objectFirTree = {
    .vertices = (vertex_t*)(float[][5]){
        {ZFLOAT(0xFFA00000), ZFLOAT(0xFFC92493), -ZFLOAT(0xFFC92493)},
        {ZFLOAT(0x00600000), ZFLOAT(0xFFC92493), -ZFLOAT(0xFFC92493)},
        {ZFLOAT(0x00000000), ZFLOAT(0x0036DB6D), -ZFLOAT(0xFE333334)},
        {ZFLOAT(0x00266666), ZFLOAT(0x00000000), -ZFLOAT(0x00000000)},
        {ZFLOAT(0xFFD9999A), ZFLOAT(0x00000000), -ZFLOAT(0x00000000)}
    },
    .vertex_count = 5,
    .surfaces = (surface_t[]){
        {3, (index_t[]){2, 3, 4}, RGBPIX(192,0,0)},
        {3, (index_t[]){0, 1, 2}, RGBPIX(0,192,0)}
    },
    .surface_count = 2
};

static const model_t objectBuilding = {
    .vertices = (vertex_t*)(float[][5]){
        {ZFLOAT(0xFF19999A), ZFLOAT(0x00000000), -ZFLOAT(0xFF266667)},
        {ZFLOAT(0xFF400000), ZFLOAT(0x00000000), -ZFLOAT(0xFF266667)},
        {ZFLOAT(0x00C00000), ZFLOAT(0x00000000), -ZFLOAT(0xFF266667)},
        {ZFLOAT(0x00E66666), ZFLOAT(0x00000000), -ZFLOAT(0xFF266667)},
        {ZFLOAT(0xFF19999A), ZFLOAT(0x00A66666), -ZFLOAT(0xFF8CCCCD)},
        {ZFLOAT(0xFF19999A), ZFLOAT(0xFF59999A), -ZFLOAT(0xFF8CCCCD)},
        {ZFLOAT(0x00E66666), ZFLOAT(0x00A66666), -ZFLOAT(0xFF8CCCCD)},
        {ZFLOAT(0x00E66666), ZFLOAT(0xFF59999A), -ZFLOAT(0xFF8CCCCD)},
        {ZFLOAT(0xFF400000), ZFLOAT(0x00800000), -ZFLOAT(0xFF666667)},
        {ZFLOAT(0xFF400000), ZFLOAT(0xFF800000), -ZFLOAT(0xFF666667)},
        {ZFLOAT(0x00C00000), ZFLOAT(0x00800000), -ZFLOAT(0xFF666667)},
        {ZFLOAT(0x00C00000), ZFLOAT(0xFF800000), -ZFLOAT(0xFF666667)},
        {ZFLOAT(0xFF400000), ZFLOAT(0x00800000), -ZFLOAT(0x01000000)},
        {ZFLOAT(0xFF400000), ZFLOAT(0xFF800000), -ZFLOAT(0x01000000)},
        {ZFLOAT(0x00C00000), ZFLOAT(0x00800000), -ZFLOAT(0x01000000)},
        {ZFLOAT(0x00C00000), ZFLOAT(0xFF800000), -ZFLOAT(0x01000000)}

    },
    .vertex_count = 16,
    .surfaces = (surface_t[]){
        {6, (index_t[]){0, 4, 6, 0, 3, 6}, RGBPIX(192,0,0)},
        {24, (index_t[]){1, 8, 9, 2, 10, 11, 8, 12, 13, 8, 9, 13, 10, 14, 15, 10, 11, 15, 9, 13, 15, 9, 11, 15}, RGBPIX(192,192,192)},
        {6, (index_t[]){0, 5, 7, 0, 3, 7}, RGBPIX(255,0,0)}
    },
    .surface_count = 3
};

static const model_t objectRocket = {
    .vertices = (vertex_t*)(float[][5]){
        {ZFLOAT(0x00000000), ZFLOAT(0x00000000), -ZFLOAT(0xFE400000)},
        {ZFLOAT(0xFFC80000), ZFLOAT(0x00380000), -ZFLOAT(0xFFD745D2)},
        {ZFLOAT(0xFFC80000), ZFLOAT(0xFFC80000), -ZFLOAT(0xFFD745D2)},
        {ZFLOAT(0x00380000), ZFLOAT(0x00380000), -ZFLOAT(0xFFD745D2)},
        {ZFLOAT(0x00380000), ZFLOAT(0xFFC80000), -ZFLOAT(0xFFD745D2)},
        {ZFLOAT(0xFF900000), ZFLOAT(0x00700000), -ZFLOAT(0x00000000)},
        {ZFLOAT(0xFF900000), ZFLOAT(0xFF900000), -ZFLOAT(0x00000000)},
        {ZFLOAT(0x00700000), ZFLOAT(0x00700000), -ZFLOAT(0x00000000)},
        {ZFLOAT(0x00700000), ZFLOAT(0xFF900000), -ZFLOAT(0x00000000)},
        {ZFLOAT(0xFFE40000), ZFLOAT(0x001C0000), -ZFLOAT(0xFF071C72)},
        {ZFLOAT(0xFFE40000), ZFLOAT(0xFFE40000), -ZFLOAT(0xFF071C72)},
        {ZFLOAT(0x001C0000), ZFLOAT(0x001C0000), -ZFLOAT(0xFF071C72)},
        {ZFLOAT(0x001C0000), ZFLOAT(0xFFE40000), -ZFLOAT(0xFF071C72)}
    },
    .vertex_count = 13,
    .surfaces = (surface_t[]){
        {12, (index_t[]){9, 1, 5, 11, 3, 7, 10, 2, 6, 12, 4, 8}, RGBPIX(255,255,0)},
        {12, (index_t[]){0, 1, 3, 0, 2, 4, 0, 1, 2, 3, 0, 4}, RGBPIX(255,0,0)}
    },
    .surface_count = 2
};

static const model_t objectSmokingRemainsLeft = {
    .vertices = (vertex_t*)(float[][5]){
        {ZFLOAT(0xFFD9999A), ZFLOAT(0x00000000), -ZFLOAT(0x00000000)},
        {ZFLOAT(0x00266666), ZFLOAT(0x00000000), -ZFLOAT(0x00000000)},
        {ZFLOAT(0x002B3333), ZFLOAT(0x00000000), -ZFLOAT(0xFFC00000)},
        {ZFLOAT(0x00300000), ZFLOAT(0x00000000), -ZFLOAT(0xFF800000)},
        {ZFLOAT(0xFFD55556), ZFLOAT(0x00000000), -ZFLOAT(0xFECCCCCD)},
    },
    .vertex_count = 5,
    .surfaces = (surface_t[]){
        {6, (index_t[]){0, 1, 3, 2, 3, 4}, RGBPIX(192,127,127)},
    },
    .surface_count = 1
};

static const model_t objectSmokingRemainsRight = {
    .vertices = (vertex_t*)(float[][5]){
        {ZFLOAT(0x002AAAAA), ZFLOAT(0x00000000), -ZFLOAT(0x00000000)},
        {ZFLOAT(0xFFD55556), ZFLOAT(0x00000000), -ZFLOAT(0x00000000)},
        {ZFLOAT(0xFFD4CCCD), ZFLOAT(0x00000000), -ZFLOAT(0xFFD00000)},
        {ZFLOAT(0xFFD00000), ZFLOAT(0x00000000), -ZFLOAT(0xFFA00000)},
        {ZFLOAT(0x002AAAAA), ZFLOAT(0x00000000), -ZFLOAT(0xFEA66667)},
    },
    .vertex_count = 5,
    .surfaces = (surface_t[]){
        {6, (index_t[]){0, 1, 3, 2, 3, 4}, RGBPIX(192,127,127)},
    },
    .surface_count = 1
};

static const model_t objectSmokingBuilding = {
    .vertices = (vertex_t*)(float[][5]){
        {ZFLOAT(0xFF400000), ZFLOAT(0x00800000), -ZFLOAT(0x01000001)},
        {ZFLOAT(0xFF400000), ZFLOAT(0xFF800000), -ZFLOAT(0x01000001)},
        {ZFLOAT(0x00C00000), ZFLOAT(0x00800000), -ZFLOAT(0x01000001)},
        {ZFLOAT(0x00C00000), ZFLOAT(0xFF800000), -ZFLOAT(0x01000001)},
        {ZFLOAT(0xFF400000), ZFLOAT(0x00800000), -ZFLOAT(0xFF99999A)},
        {ZFLOAT(0x00C00000), ZFLOAT(0xFF800000), -ZFLOAT(0xFFB33334)}
    },
    .vertex_count = 6,
    .surfaces = (surface_t[]){
        {18, (index_t[]){0, 1, 2, 1, 2, 3, 0, 2, 4, 0, 1, 4, 2, 3, 5, 1, 3, 5}, RGBPIX(127,127,127)},
    },
    .surface_count = 1
};

static const model_t objectSmokingGazebo = {
    .vertices = (vertex_t*)(float[][5]){
        {ZFLOAT(0x00000000), ZFLOAT(0xFFF00000), -ZFLOAT(0xFF8CCCCD)},
        {ZFLOAT(0x00199999), ZFLOAT(0xFFF00000), -ZFLOAT(0xFF8CCCCD)},
        {ZFLOAT(0x00800000), ZFLOAT(0x00800000), -ZFLOAT(0x00000000)},
        {ZFLOAT(0xFF800000), ZFLOAT(0x00800000), -ZFLOAT(0x00000000)},
        {ZFLOAT(0x00800000), ZFLOAT(0xFF800000), -ZFLOAT(0x00000000)},
        {ZFLOAT(0xFF800000), ZFLOAT(0xFF800000), -ZFLOAT(0x00000000)}
    },
    .vertex_count = 6,
    .surfaces = (surface_t[]){
        {12, (index_t[]){0, 1, 2, 0, 1, 3, 0, 1, 4, 0, 1, 5}, RGBPIX(127,127,127)},
    },
    .surface_count = 1
};

typedef struct {
    const model_t* model;
    float hitbox_height;
    float hitbox_radius;
} object_t;

static object_t object_models[24] = {
    {NULL}, //objectPyramid,
    {&objectSmallLeafyTree},
    {&objectTallLeafyTree},
    {&objectSmallLeafyTree},
    {&objectSmallLeafyTree},
    {&objectGazebo},
    {&objectTallLeafyTree},
    {&objectFirTree},
    {&objectBuilding},
    {&objectRocket},
    {&objectRocket},
    {&objectRocket},
    {&objectRocket},
    {&objectSmokingRemainsRight},
    {&objectSmokingRemainsLeft},
    {&objectSmokingRemainsLeft},
    {&objectSmokingRemainsLeft},
    {&objectSmokingGazebo},
    {&objectSmokingRemainsRight},
    {&objectSmokingRemainsRight},
    {&objectSmokingBuilding},
    {&objectSmokingRemainsRight},
    {&objectSmokingRemainsLeft},
    {&objectSmokingRemainsLeft},
};

static const int destruction_offset = 12;
static const int objects_placed = 8192;

#undef ZFLOAT

uint8_t objects_map[256][256];

static void calculate_hitbox(object_t* object) {
    object->hitbox_height = 0;
    object->hitbox_radius = 0;

    if (!object->model) {
        return;
    }

    for (int i = 0; i < object->model->vertex_count; ++i) {
        object->hitbox_height = max(object->hitbox_height, object->model->vertices[i].position.z);
        object->hitbox_radius = max(object->hitbox_radius, vec2_length(object->model->vertices[i].position.v));
    }
}

void objects_init(void) {
    for (int i = 0; i < count_of(object_models); ++i) {
        calculate_hitbox(&object_models[i]);
    }

    memset(objects_map, 0xff, sizeof(objects_map));
    for (int i = 0; i < objects_placed; ++i) {
        for (int j = 0; j < 16; ++j) {
            uint8_t x = rand()&0xff;
            uint8_t y = rand()&0xff;
            if (x >= 8 && y >= 9 && objects_map[y][x] == 0xff) {
                objects_map[y][x] = (rand()&0x7) + 1;
                break;
            }
        }
    }

    for (int i = 0; i < 3; ++i) {
        objects_map[1+i][7] = 9;
    }
}

static float* random_cylinder(float height, float radius) {
    static float cylinder[VEC3_SIZE];
    
    do {
        cylinder[0] = (float)rand()/(float)RAND_MAX;
        cylinder[1] = (float)rand()/(float)RAND_MAX;
    } while (vec2_length_squared(cylinder) > 1.0f);
    
    vec2_multiply_f(cylinder, cylinder, radius);

    cylinder[2] = (float)rand()/(float)RAND_MAX * height;

    return cylinder;
}

static void object_destroy(int ox, int oy, float* position) {
    ox &= 0xff;
    oy &= 0xff;
    uint8_t object = objects_map[oy][ox];
    if (object < count_of(object_models) && object_models[object].model) {
        int clusters;

        if (object < count_of(object_models) - destruction_offset) {
            objects_map[oy][ox] += destruction_offset;
            clusters = 20*4;
        } else {
            clusters = 3*4;
        }

        float source[VEC3_SIZE];
        float height = object_models[object].hitbox_height * 0.8f;
        float radius = object_models[object].hitbox_radius * 0.5f;
        for (int i = 0; i < clusters; ++i) {
            vec3_add(source, position, random_cylinder(height, radius));
            particles_add_explosion(source, 2);
        }
    }
}

bool objects_hit_and_destroy(float* position) {
    struct vec2i coord = {.x=(int)roundf(position[0]), .y=(int)roundf(position[1])};
    for (int y = coord.y - 1; y <= coord.y + 1; ++y) {
        for (int x = coord.x - 1; x <= coord.x + 1; ++x) {
            uint8_t object = objects_map[y&0xff][x&0xff];
            if (object < count_of(object_models) && object_models[object].hitbox_radius) {
                vec3_t objpos = {.x=x, .y=y};
                objpos.z = terrain_get_altitude(objpos.x, objpos.y);

                if (!terrain_is_water(objpos.z)) {
                    if (position[2] <= objpos.z + object_models[object].hitbox_height) {
                        if (vec2_distance_squared(objpos.v, position) <= sqrf(object_models[object].hitbox_radius)) {
                            object_destroy(x, y, objpos.v);
                            return true;
                        }
                    }
                }
            }
        }
    }

    return false;
}

void objects_update(float dt) {
}

static void draw_voxel(pixel_t* volume, const int* coordinate, pixel_t colour) {
    if (coordinate[2] > HEIGHT_MAP_OBJECT(coordinate[0], coordinate[1])) {
        HEIGHT_MAP_OBJECT(coordinate[0], coordinate[1]) = coordinate[2];
    }

    if (coordinate[2] > HEIGHT_MAP_TERRAIN(coordinate[0], coordinate[1])) {
        volume[VOXEL_INDEX(coordinate[0], coordinate[1], coordinate[2])] = colour;
    }
    if ((uint8_t)HEIGHT_MAP_TERRAIN(coordinate[0], coordinate[1]) < VOXELS_Z) {
        volume[VOXEL_INDEX(coordinate[0], coordinate[1], HEIGHT_MAP_TERRAIN(coordinate[0], coordinate[1]))] = 0;
    }
}

void objects_draw(pixel_t* volume) {
    
    int tile0[VEC2_SIZE] = {
        (int)floorf((-(VOXELS_X-1)*0.5f) / world_scale + world_position.x),
        (int)floorf((-(VOXELS_Y-1)*0.5f) / world_scale + world_position.y)
    };
    int tiles = (int)ceilf((float)VOXELS_X / world_scale);
    
    graphics_draw_voxel_cb = draw_voxel;

    float world[MAT4_SIZE];
    mat4_identity(world);
    mat4_apply_translation(world, (float[3]){VOXELS_X/2, VOXELS_Y/2, 0});
    mat4_apply_scale(world, world_scale);
    mat4_apply_translation(world, (float[3]){-world_position.x, -world_position.y, -world_position.z});

    vec3_t position;
    float matrix[MAT4_SIZE];

    //float rotation[MAT4_SIZE];
    //mat4_identity(rotation);
    
    for (int y = tile0[1]; y <= tile0[1]+tiles; ++y) {
        position.y = y;

        for (int x = tile0[0]; x <= tile0[0]+tiles; ++x) {
            uint8_t object = objects_map[y&0xff][x&0xff];
            if (object < count_of(object_models) && object_models[object].model) {
                position.x = x;
                position.z = terrain_get_altitude(position.x, position.y);
                if (!terrain_is_water(position.z)) {
                    mat4_multiply(matrix, world, (float[MAT4_SIZE]){1,0,0,0, 0,1,0,0, 0,0,1,0, position.x,position.y,position.z,1});
                    //mat4_rotation_z(rotation, (x*257+y*17)*11.03f);
                    //mat4_multiply(matrix, matrix, rotation);
                    model_draw(volume, object_models[object].model, matrix);
                }
            }
        }
    }

    graphics_draw_voxel_cb = NULL;

}

#pragma GCC pop_options
