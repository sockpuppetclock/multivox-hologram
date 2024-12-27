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
#include <linux/joystick.h>
#include <poll.h>
#include <dirent.h>
#include <termios.h>

#include "array.h"
#include "mathc.h"
#include "rammel.h"
#include "input.h"
#include "gadget.h"
#include "graphics.h"
#include "model.h"

#include "flight_tiles.h"

volume_double_buffer_t* volume_buffer;
pixel_t* scratch_volume = NULL;

static array_t tile_set = {sizeof(model_t*)};

typedef struct {
    model_t* model;
    float position[VEC3_SIZE];
    float rotation[VEC3_SIZE];
    float scale;
} tiledef_t;

#define TILE_ROWS 10
#define TILES_WIDE 10
static tiledef_t tile_defs[TILE_ROWS][TILES_WIDE];
static int first_row = 0;

static void load_tiles(void) {
    array_reserve(&tile_set, 256);
    array_clear(&tile_set);

    // if there's a directory called tiles with a bunch of .objs in it, load them all up
    char tiles_dir[1024] = "tiles/";
    char* filename = &tiles_dir[strlen(tiles_dir)];
    DIR* dir = opendir(tiles_dir);
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            const char* ext = strchr(entry->d_name, '.');
            if (ext && strcmp(ext, ".obj") == 0) {
                strncpy(filename, entry->d_name, tiles_dir + sizeof(tiles_dir) - 2 - filename);
                array_resize(&tile_set, tile_set.count+1);
                
                model_t* model = model_load(tiles_dir, STYLE_WIREFRAME_ALWAYS);
                model_set_colour(model, RGBPIX(127,255,255));
                ((model_t**)tile_set.data)[tile_set.count-1] = model;
            }
        }
    }

#ifdef TILES_COMPILED
    // add any compiled-in tiles
    size_t tile0 = tile_set.count;
    array_resize(&tile_set, tile_set.count + count_of(tile_models));
    for (int i = 0; i < count_of(tile_models); ++i) {
        ((model_t**)tile_set.data)[tile0 + i] = (model_t*)&tile_models[i];
    }
#endif
}

static float wall_rotations[4][VEC3_SIZE] = {
    { M_PI_2, M_PI_2, 0},
    {      0, M_PI_2, 0},
    {-M_PI_2, M_PI_2, 0},
    { M_PI,   M_PI_2, 0}
};
static float floor_rotations[4][VEC3_SIZE] = {
    {0, 0, 0},
    {0, 0, M_PI_2},
    {0, 0, M_PI},
    {0, 0,-M_PI_2}
};

static void step_tiles(void) {
    const float tile_size = 10.0f;
    const float tile_scale = 0.95f / tile_size;
    const float far_row = TILE_ROWS * 0.5f;

    const float wall_x = 1.2f;
    const float wall_z = 0.1f;
    const float floor_z = -1.0f;
    const float surface_x = 1.7f;

    int back_row = first_row;
    int double_back = (back_row + TILE_ROWS - 1) % TILE_ROWS;
    first_row = (first_row + 1) % TILE_ROWS;

    int tidx = 0;

    tiledef_t* tile;

    // walls
    for (int i = 0; i < 2; ++i) {
        float side = (i*2 - 1);
        if (tile_defs[double_back][tidx].model && !tile_defs[double_back][tidx+1].model) {
            // occupied by a double
            tile_defs[back_row][tidx  ].model = NULL;
            tile_defs[back_row][tidx+1].model = NULL;
        } else {
            if ((rand() & 0xc0) == 0) {
                // double
                tile = &tile_defs[back_row][tidx];
                tile->model = ((model_t**)tile_set.data)[rand() % tile_set.count];
                vec3(tile->position, wall_x * side, far_row + 0.5f, wall_z);
                vec3_assign(tile->rotation, wall_rotations[(rand() >> 11) & 3]);
                tile->scale = tile_scale * 2.0f * -side;
                
                tile_defs[back_row][tidx+1].model = NULL;
            } else {
                // singles
                tile = &tile_defs[back_row][tidx];
                tile->model = ((model_t**)tile_set.data)[rand() % tile_set.count];
                vec3(tile->position, wall_x * side, far_row, wall_z + 0.5f);
                vec3_assign(tile->rotation, wall_rotations[(rand() >> 11) & 3]);
                tile->scale = tile_scale * -side;

                tile = &tile_defs[back_row][tidx+1];
                tile->model = ((model_t**)tile_set.data)[rand() % tile_set.count];
                vec3(tile->position, wall_x * side, far_row, wall_z - 0.5f);
                vec3_assign(tile->rotation, wall_rotations[(rand() >> 11) & 3]);
                tile->scale = tile_scale * -side;
            }
        }

        tidx += 2;
    }

    // floor
    for (int i = 0; i < 2; ++i) {
        float side = (i*2 - 1);

        tile = &tile_defs[back_row][tidx++];
        tile->model = ((model_t**)tile_set.data)[rand() % tile_set.count];
        vec3(tile->position, side * 0.5f, far_row, floor_z);
        vec3_assign(tile->rotation, floor_rotations[(rand() >> 11) & 3]);
        tile->scale = tile_scale;
    }

    // surface
    for (int side = -1; side <= 1; side += 2) {
        for (int i = 0; i < 2; ++i) {
            tile = &tile_defs[back_row][tidx++];
            tile->model = ((model_t**)tile_set.data)[rand() % tile_set.count];
            vec3(tile->position, (surface_x + i) * side, far_row, 1.0f);
            vec3_assign(tile->rotation, floor_rotations[(rand() >> 11) & 3]);
            tile->scale = tile_scale;
        }
    }




    for (int r = 0; r < TILE_ROWS-1; ++r) {
        int row = (first_row + r) % TILE_ROWS;
        for (int x = 0; x < TILES_WIDE; ++x) {
            tile = &tile_defs[row][x];
            if (tile->model) {
                tile->position[1] -= 1;
            }
        }
    }
}


int main(int argc, char** argv) {

    int fd = shm_open("/rotovox_double_buffer", O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open");
        exit(1);
    }

    volume_buffer = mmap(NULL, sizeof(*volume_buffer), PROT_WRITE, MAP_SHARED, fd, 0);
    if (volume_buffer == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    load_tiles();
    if (tile_set.count <= 0) {
        printf("failed to load tiles\n");
        exit(1);
    }

    srand(time(NULL));
    memset(tile_defs, 0, sizeof(tile_defs));
    for (int i = 0; i < TILE_ROWS; ++i) {
        step_tiles();
    }

    float volume_centre[VEC3_SIZE] = {VOXELS_X/2, VOXELS_Y/2, VOXELS_Z/2};
    float world_rotation[VEC3_SIZE] = {0, 0, 0};
    float world_position[VEC3_SIZE] = {0, 0, 0};

//    float zoom = 32.0f;
    float delta = 0.05f;
    float wobble = 0.0f;

    input_set_nonblocking();

    for (int ch = 0; ch != 27; ch = getchar()) {
        uint8_t page = !volume_buffer->page;
        pixel_t* content = volume_buffer->volume[page];

        memset(content, 0, sizeof(volume_buffer->volume[page]));
        //fade_buffer(volume_buffer->volume[page], volume_buffer->volume[!page]);

        float zoom = 24.0f;

        world_position[1] -= delta;
        if (world_position[1] < 0) {
            world_position[1] += 1;
            step_tiles();
        }

        wobble = fmodf(wobble + 0.03f, M_PI*2);
        world_rotation[1] = sinf(wobble) * 0.06f;
        
        float world[MAT4_SIZE];
        mat4_identity(world);
        mat4_apply_translation(world, volume_centre);
        mat4_apply_scale(world, zoom);
        mat4_apply_translation(world, world_position);
        mat4_apply_rotation(world, world_rotation);

        float matrix[MAT4_SIZE];
        for (int y = 0; y < TILE_ROWS; ++y) {
            for (int x = 0; x < TILES_WIDE; ++x) {
                tiledef_t* tile = &tile_defs[y][x];
                if (tile->model) {
                    memcpy(matrix, world, sizeof(matrix));
                    mat4_apply_translation(matrix, tile->position);
                    mat4_apply_rotation(matrix, tile->rotation);
                    mat4_apply_scale(matrix, tile->scale);
                    model_draw(content, tile->model, matrix);
                }
            }
        }

        volume_buffer->page = page;
        usleep(50000);
    }

    munmap(volume_buffer, sizeof(*volume_buffer));
    close(fd);

    return 0;
}




