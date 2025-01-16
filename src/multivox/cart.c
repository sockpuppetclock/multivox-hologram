#include "cart.h"

#include <math.h>
#include <stdlib.h>

#include "mathc.h"
#include "model.h"
#include "rammel.h"

static model_t cart_model = {
    .vertex_count = 24,
    .vertices = (vertex_t*)(float[][5]){
        {16.3145, 2, 15.2947}, {20, 2, 18.9803}, {20, 2, 24}, {-20, 2, 24}, {-20, 2, 18.9803}, {-16.3145, 2, 15.2947}, {-16.3145, 2, 9.86573}, {-20, 2, 6.18021}, 
        {-20, 2, -16}, {20, 2, -16}, {20, 2, -6.7141}, {16.3145, 2, -3.02859}, {20, -2, 18.9803}, {20, -2, 24}, {-20, -2, 24}, {-20, -2, 18.9803}, 
        {-16.3145, -2, 15.2947}, {-16.3145, -2, 9.86573}, {-20, -2, 6.18022}, {-20, -2, -16}, {20, -2, -16}, {20, -2, -6.7141}, {16.3145, -2, -3.02859}, {16.3145, -2, 15.2947}, 
    },
    .surface_count = 1,
    .surfaces = (surface_t[]){
        {132, (index_t[]){
            1, 2, 0, 0, 2, 3, 0, 3, 5, 5, 3, 4, 5, 6, 0, 0, 6, 11, 11, 6, 8, 11, 8, 9, 6, 7, 8, 9, 10, 11, 12, 13, 1, 1, 13, 2, 13, 14, 2, 2, 
            14, 3, 14, 15, 3, 3, 15, 4, 15, 16, 4, 4, 16, 5, 16, 17, 5, 5, 17, 6, 17, 18, 6, 6, 18, 7, 18, 19, 7, 7, 19, 8, 19, 20, 8, 8, 20, 9, 20, 21, 
            9, 9, 21, 10, 21, 22, 10, 10, 22, 11, 22, 23, 11, 11, 23, 0, 23, 12, 0, 0, 12, 1, 12, 23, 13, 13, 23, 14, 14, 23, 16, 14, 16, 15, 16, 23, 17, 17, 23, 22, 
            17, 22, 19, 19, 22, 20, 20, 22, 21, 19, 18, 17, 
        }, RGBPIX(255, 255, 255)},
    },
};



static float slot_height(float a) {
    return powf(0.5f * (1 + cosf(a)), 10) * 24 - 12;
}

void cart_draw(cart_t* cart, pixel_t* volume, float slot_angle) {
    float matrix[MAT4_SIZE];

    mat4_identity(matrix);
    mat4_apply_translation(matrix, (float[3]){(VOXELS_X-1)*0.5f, (VOXELS_Y-1)*0.5f, 0});
    mat4_apply_rotation_z(matrix, slot_angle);
    mat4_apply_translation(matrix, (float[3]){40.0f, 4.0f, slot_height(slot_angle)});

    cart_model.surfaces[0].colour = cart->colour;
    model_draw(volume, &cart_model, matrix);

    int z0 = abs(slot_angle * 100);
    const int m = 1;
    if (cart->voxel_shot[m]) {
        for (int y = 0; y < VOXELS_Y>>m; ++y) {
            int vy = y+(VOXELS_Y/2)-(VOXELS_Y>>(m+1));
            for (int x = 0; x < VOXELS_X>>m; ++x) {
                for (int z = 0; z < VOXELS_Z>>m; ++z) {
                    int vz = z0 + z+(VOXELS_Z)-(VOXELS_Z>>m);
                    if ((uint)vz < VOXELS_Z) {
                        volume[VOXEL_INDEX(x, vy, vz)] = cart->voxel_shot[m][(x * (VOXELS_Z>>m)) + (y * (VOXELS_X>>m) * (VOXELS_Z>>m)) + z];
                    }
                }
            }
        }
    }
    
}

