#include "carousel.h"

#include <math.h>
#include <stdio.h>

#include "model.h"
#include "input.h"
#include "rammel.h"

static const model_t cart_model = {
    .vertex_count = 24,
    .vertices = (vertex_t*)(float[][5]){
        {1.63145, 0.4, 1.52947}, {2, 0.4, 1.89803}, {2, 0.4, 2.4}, {-2, 0.4, 2.4}, {-2, 0.4, 1.89803}, {-1.63145, 0.4, 1.52947}, {-1.63145, 0.4, 0.986573}, {-2, 0.4, 0.618021}, 
        {-2, 0.4, -1.6}, {2, 0.4, -1.6}, {2, 0.4, -0.67141}, {1.63145, 0.4, -0.302859}, {2, -0.4, 1.89803}, {2, -0.4, 2.4}, {-2, -0.4, 2.4}, {-2, -0.4, 1.89803}, 
        {-1.63145, -0.4, 1.52947}, {-1.63145, -0.4, 0.986573}, {-2, -0.4, 0.618022}, {-2, -0.4, -1.6}, {2, -0.4, -1.6}, {2, -0.4, -0.67141}, {1.63145, -0.4, -0.302859}, {1.63145, -0.4, 1.52947}, 
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

typedef struct {
    const char* command;
    const char* arguments;
    const char* workingdir;
    const char* environment;
} cart_t;

static cart_t carts[] = {
    {
        .command = "/home/pi/development/multivox/build/viewer",
        .arguments = "/home/pi/Multivox/models/koi/koiscene.obj /home/pi/Multivox/models/dreamchaser.obj /home/pi/Multivox/models/mantisshrimp/mantisshrimp.obj /home/pi/Multivox/models/smart/smart.obj /home/pi/Multivox/models/holochess/scene.obj /home/pi/Multivox/models/tutankhamun/tutankhamun.obj /home/pi/Multivox/models/slimer/slimer.obj /home/pi/Multivox/models/xwing/xwing.obj",
    },
    {
        .command = "/home/pi/development/multivox/build/tesseract",
    },
    {
        .command = "/home/pi/development/multivox/build/zander",
    },
    {
        .command = "/home/pi/development/doom/doomvox/doomvox/doomvox",
        .workingdir = "/home/pi/development/doom/doomvox",
        .arguments = "-iwad DOOM.WAD"
    },
    {
        .command = "/home/pi/development/gta/GTA3/re3-rel-oal",
        .workingdir = "/home/pi/development/gta/GTA3/",
        .environment = "DISPLAY=:0.0"
    },


};

static float selection_target;
static float selection_current;

void carousel_init() {
    selection_target = 0;
    selection_current = 0;
}

void carousel_update(float dt) {
    const float speed = 3.0f;

    if (input_get_button(0, BUTTON_RB, BUTTON_PRESSED)) {
        selection_target = min(selection_target + 1, (float)count_of(carts));
    }
    if (input_get_button(0, BUTTON_LB, BUTTON_PRESSED)) {
        selection_target = max(selection_target - 1, 0.0f);
    }

    if (selection_target > selection_current) {
        selection_current = min(selection_target, selection_current + dt * speed);
    } else if (selection_target < selection_current) {
        selection_current = max(selection_target, selection_current - dt * speed);
    }

    printf("%g %g\n", selection_target, selection_current);
}

void carousel_draw(pixel_t* volume) {

}

