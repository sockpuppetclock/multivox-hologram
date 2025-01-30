#include "carousel.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <linux/limits.h>

#include "multivox.h"
#include "input.h"
#include "rammel.h"
#include "timer.h"
#include "cart.h"
#include "array.h"


static array_t carts = {sizeof(cart_t)};


static int selection_target;
static float selection_current;


static void load_carts(const char* directory) {
    DIR* dir;
    struct dirent* entry;

    if ((dir = opendir(directory)) == NULL) {
        perror("opendir");
        return;
    }

    while ((entry = readdir(dir))) {
        size_t namelen = strlen(entry->d_name);
        if (namelen > 4 && strcmp(entry->d_name + namelen - 4, ".mct") == 0) {
            char filepath[PATH_MAX];
            if (snprintf(filepath, sizeof(filepath), "%s/%s", directory, entry->d_name) >= 0) {
                if (!cart_load(array_push(&carts), filepath)) {
                    array_pop(&carts);
                }
            }
        }
    }

    closedir(dir);
}

void carousel_init() {
    selection_target = 0;
    selection_current = 0;

    char directory_string[PATH_MAX];
    const char* directory = getenv("MULTIVOX_CART_PATH");
    if (!directory) {
        const char* home = getenv("HOME");
        if (!home) {
            home = "/home/pi";
        }
        snprintf(directory_string, sizeof(directory_string), "%s/Multivox/carts", home);
        directory = directory_string;
    }

    load_carts(directory);
}

void carousel_update(float dt) {
    const float speed = 3.0f;

    if (input_get_button(0, BUTTON_RB, BUTTON_PRESSED)) {
        selection_target = min(selection_target + 1, carts.count - 1);
    }
    if (input_get_button(0, BUTTON_LB, BUTTON_PRESSED)) {
        selection_target = max(selection_target - 1, 0);
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
        cart_action_t action = multivox_cart_execute(array_get(&carts, selection_target));
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

    for (int i = 0; i < carts.count; ++i) {
        float angle =  atan((i - smooth) * 0.5f) * 1.2f;
        cart_draw(array_get(&carts, i), volume, angle);
    }
}

