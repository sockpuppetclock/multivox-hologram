#include "carousel.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "multivox.h"
#include "input.h"
#include "rammel.h"
#include "timer.h"
#include "cart.h"

static cart_t carts[] = {
    {
        .colour = RGBPIX(255,255,0),
        .command = "/home/pi/development/multivox/build/viewer",
        .arguments = "/home/pi/Multivox/models/koi/koiscene.obj /home/pi/Multivox/models/dreamchaser.obj /home/pi/Multivox/models/mantisshrimp/mantisshrimp.obj /home/pi/Multivox/models/smart/smart.obj /home/pi/Multivox/models/holochess/scene.obj /home/pi/Multivox/models/tutankhamun/tutankhamun.obj /home/pi/Multivox/models/slimer/slimer.obj /home/pi/Multivox/models/xwing/xwing.obj",
    },
    {
        .colour = RGBPIX(255,0,0),
        .command = "/home/pi/development/multivox/build/tesseract",
    },
    {
        .colour = RGBPIX(0,128,128),
        .command = "/home/pi/development/multivox/build/eighty",
    },
    {
        .colour = RGBPIX(0,255,0),
        .command = "/home/pi/development/multivox/build/zander",
    },
    {
        .colour = RGBPIX(127,127,127),
        .command = "/home/pi/development/multivox/build/flight",
    },
    {
        .colour = RGBPIX(0,0,255),
        .command = "/home/pi/development/doom/doomvox/doomvox/doomvox",
        .workingdir = "/home/pi/development/doom/doomvox",
        .arguments = "-iwad DOOM.WAD"
    },
    {
        .colour = RGBPIX(255,128,0),
        .command = "/home/pi/development/gta/GTA3/re3-rel-oal",
        .workingdir = "/home/pi/development/gta/GTA3",
        .environment = "DISPLAY=:0.0 XAUTHORITY=/home/pi/.Xauthority"
    },

};

static uint cart_count = count_of(carts);

static int selection_target;
static float selection_current;


void carousel_init() {
    selection_target = 0;
    selection_current = 0;
}

void carousel_update(float dt) {
    const float speed = 3.0f;

    if (input_get_button(0, BUTTON_RB, BUTTON_PRESSED)) {
        selection_target = min(selection_target + 1, (float)cart_count - 1);
    }
    if (input_get_button(0, BUTTON_LB, BUTTON_PRESSED)) {
        selection_target = max(selection_target - 1, 0.0f);
    }

    float target = (float)selection_target;
    if (target > selection_current) {
        selection_current = min(target, selection_current + dt * speed);
    } else if (target < selection_current) {
        selection_current = max(target, selection_current - dt * speed);
    }

    if (input_get_button(0, BUTTON_VIEW, BUTTON_PRESSED)) {
        cart_action_t action = multivox_cart_resume();
        (void)action;
        return;
    }
    if (input_get_button(0, BUTTON_A, BUTTON_PRESSED)) {
        cart_action_t action = multivox_cart_execute(&carts[selection_target]);
        (void)action;
        return;
    }

    if (input_get_button(0, BUTTON_UP, BUTTON_PRESSED)) {
        voxel_buffer->bits_per_channel = clamp(voxel_buffer->bits_per_channel + 1, 1, 3);
    }
    if (input_get_button(0, BUTTON_DOWN, BUTTON_PRESSED)) {
        voxel_buffer->bits_per_channel = clamp(voxel_buffer->bits_per_channel - 1, 1, 3);
    }


}

static float ease_turn(float a) {
    float a2 = a * a;
    return a2 / (2.0f * (a2 - a) + 1.0f);
}

void carousel_draw(pixel_t* volume) {
    float smooth = floorf(selection_current) + ease_turn(fmodf(selection_current, 1.0f));

    for (int i = 0; i < cart_count; ++i) {
        float angle =  atan((i - smooth) * 0.5f) * 1.2f;
        cart_draw(&carts[i], volume, angle);
    }
}

