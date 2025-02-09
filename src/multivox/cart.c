#include "cart.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

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
        }, HEXPIX(FFFFFF)},
    },
};


static char* key_value(char* line, const char* key) {

    const char whitespace[] = " \f\n\r\t\v";
    char* token = line + strspn(line, whitespace);

    size_t keylen = strlen(key);
    if (strncmp(token, key, keylen) == 0) {
        token = strchr(token, '=');
        if (token && *++token) {
            token += strspn(token, whitespace);
            
            size_t len = strlen(token);
            while (len > 0 && strchr(whitespace, token[len-1])) {
                --len;
            }
            char* value = malloc(len + 1);
            memcpy(value, token, len);
            value[len] = '\0';
            return value;
        }
    }

    return NULL;
}

static float slot_height(float a) {
    return powf(0.5f * (1 + cosf(a)), 80) * 24 - 12;
}

void cart_grab_voxshot(cart_t* cart, const pixel_t* volume) {
    if (!cart->voxel_shot[0]) {
        cart->voxel_shot[0] = malloc(VOXELS_COUNT * sizeof(pixel_t));
        memset(cart->voxel_shot[0], 0, VOXELS_COUNT * sizeof(pixel_t));
    }

    for (int y = 0; y < VOXELS_Y; ++y) {
        for (int x = 0; x < VOXELS_X; ++x) {
            if (voxel_in_cylinder(x, y)) {
                for (int z = 0; z < VOXELS_Z; ++z) {
                    cart->voxel_shot[0][x * VOXELS_Z + y * VOXELS_X * VOXELS_Z + z] = volume[VOXEL_INDEX(x, y, z)];
                }
            }
        }
    }

    int count[3] = {VOXELS_X, VOXELS_Y, VOXELS_Z};
    for (int m = 1; m < count_of(cart->voxel_shot); ++m) {
        count[0] /= 2;
        count[1] /= 2;
        count[2] /= 2;

        if (!cart->voxel_shot[m]) {
            cart->voxel_shot[m] = malloc(count[0] * count[1] * count[2] * sizeof(pixel_t));
            memset(cart->voxel_shot[m], 0, count[0] * count[1] * count[2] * sizeof(pixel_t));
        }

        for (int y = 0; y < count[1]; ++y) {
            for (int x = 0; x < count[0]; ++x) {
                for (int z = 0; z < count[2]; ++z) {

                    int rgb[3] = {0,0,0};
                    for (int j = 0; j < 2; ++j) {
                        for (int i = 0; i < 2; ++i) {
                            for (int k = 0; k < 2; ++k) {
                                pixel_t colour = cart->voxel_shot[m - 1][((x*2+i) * count[2]*2) + ((y*2+j) * count[0]*count[2]*4) + (z*2+k)];
                                rgb[0] += R_PIX(colour);
                                rgb[1] += G_PIX(colour);
                                rgb[2] += B_PIX(colour);
                            }
                        }
                    }

                    rgb[0] = min(255, rgb[0] / 3);
                    rgb[1] = min(255, rgb[1] / 3);
                    rgb[2] = min(255, rgb[2] / 3);

                    cart->voxel_shot[m][(x * count[2]) + (y * count[0]*count[2]) + (z)] = RGBPIX(rgb[0], rgb[1], rgb[2]);
                }
            }
        }
    }

}

static void try_load_voxshot(cart_t* cart, char* filename) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        return;
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        perror("fstat");
        close(fd);
        return;
    }

    size_t size = sb.st_size;
    if (size != VOXELS_COUNT) {
        close(fd);
        return;
    }

    void* mapped = mmap(0, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return;
    }

    cart_grab_voxshot(cart, mapped);

    munmap(mapped, size);
    close(fd);
}

void cart_save_voxshot(cart_t* cart) {
    if (!cart->cartpath || !cart->voxel_shot[0]) {
        return;
    }

    size_t namelen = strlen(cart->cartpath);
    if (namelen > 4 && cart->cartpath[namelen-4] == '.') {
        char shotname[namelen+1];
        memcpy(shotname, cart->cartpath, namelen - 4);
        memcpy(shotname + namelen - 4, ".rvx", 5);

        FILE* fd = fopen(shotname, "wb");
        if (!fd) {
            perror("save");
        }

        fwrite(cart->voxel_shot[0], sizeof(pixel_t), VOXELS_COUNT, fd);
        fclose(fd);
    }
}

bool cart_load(cart_t* cart, char* filename) {
    memset(cart, 0, sizeof(*cart));

    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("fopen");
        return false;
    }

    size_t namelen = strlen(filename);

    char* cartpath = malloc(namelen + 1);
    memcpy(cartpath, filename, namelen);
    cartpath[namelen] = '\0';
    cart->cartpath = cartpath;
    
    cart->colour = HEXPIX(FF00FF);

    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        if (!cart->command) cart->command = key_value(line, "command");
        if (!cart->arguments) cart->arguments = key_value(line, "arguments");
        if (!cart->workingdir) cart->workingdir = key_value(line, "workingdir");
        if (!cart->environment) cart->environment = key_value(line, "environment");
        
        char* value;
        if ((value = key_value(line, "colour"))) {
            int r, g, b;
            if (sscanf(value, "#%02x%02x%02x", &r, &g, &b) == 3) {
                cart->colour = RGBPIX(r, g, b);
            }
            free(value);
        }
    }

    fclose(file);

    if (namelen > 4 && filename[namelen-4] == '.') {
        char shotname[namelen+1];
        memcpy(shotname, filename, namelen - 4);
        memcpy(shotname + namelen - 4, ".rvx", 5);
        
        try_load_voxshot(cart, shotname);
    }

    
    return true;
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
                    int vz = z0 + z+(VOXELS_Z)-(VOXELS_Z>>m)*3/2;
                    if ((uint)vz < VOXELS_Z) {
                        volume[VOXEL_INDEX(x, vy, vz)] = cart->voxel_shot[m][(x * (VOXELS_Z>>m)) + (y * (VOXELS_X>>m) * (VOXELS_Z>>m)) + z];
                    }
                }
            }
        }
    }
    
}

