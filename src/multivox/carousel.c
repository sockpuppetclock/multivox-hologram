#define _GNU_SOURCE
#include "carousel.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/joystick.h>

#include "model.h"
#include "input.h"
#include "rammel.h"
#include "timer.h"

#define CART_SLOTS 7

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

typedef struct {
    const char* command;
    const char* arguments;
    const char* workingdir;
    const char* environment;

    pixel_t colour;
} cart_t;

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
        .colour = RGBPIX(0,255,0),
        .command = "/home/pi/development/multivox/build/zander",
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

typedef struct {
    uint32_t cart_id;
    pid_t process_id;

} cart_context_t;

static cart_context_t cart_context = {.process_id = -1};

static int selection_target;
static float selection_current;


static bool background_process() {
    input_update();

    if (input_get_button(0, BUTTON_VIEW, BUTTON_PRESSED)) {
        return false;
    }

    usleep(100000);

    return true;
}

static void background_loop(pid_t pid) {
    int status;

    while (true) {
        pid_t result = waitpid(pid, &status, WNOHANG);

        if (result == 0) {
            if (!background_process()) {
                printf("suspend\n");
                if (kill(cart_context.process_id, SIGSTOP) == 0) {
                    break;
                } else {
                    perror("suspend");
                }
            }

        } else if (result == -1) {
            perror("waitpid");
            break;
            
        } else {
            if (WIFEXITED(status)) {
                printf("Child process exited with status %d\n", WEXITSTATUS(status));

            } else if (WIFSIGNALED(status)) {
                printf("Child process killed by signal %d\n", WTERMSIG(status));

            }
            break;
        }
    }
}

static void cart_execute(uint32_t cart_id) {
    static uint32_t nospam = 0;
    if ((timer_frame_count - nospam) < 10) {
        printf("nospam\n");
        return;
    }
    nospam = timer_frame_count;

    if (cart_id >= cart_count) {
        return;
    }
    cart_t* cart = &carts[cart_id];
    if (!cart->command) {
        return;
    }

    #define IF_STRING(str) ((str) ? (str) : "")
    printf("cartecute %s %s %s\n", IF_STRING(cart->command), IF_STRING(cart->workingdir), IF_STRING(cart->environment));

    if (cart_context.process_id > 0) {
        if (cart_context.cart_id == cart_id) {
            printf("resume\n");
            if (kill(cart_context.process_id, SIGCONT) == 0) {
                background_loop(cart_context.process_id);
                return;
            }
            perror("process");
        }

        printf("kill\n");
        kill(cart_context.process_id, SIGKILL);
    }

    pid_t pid = fork();
    cart_context.process_id = pid;
    cart_context.cart_id = cart_id;

    printf("pid %d\n", pid);

    if (pid == -1) {
        perror("fork");
        return;
    }

    if (pid > 0) {
        background_loop(pid);

    } else {
        //execute the cart

        if (cart->workingdir) {
            chdir(cart->workingdir);
        }

        size_t argslen = cart->arguments ? strlen(cart->arguments) : 0;
        size_t envslen = cart->environment ? strlen(cart->environment) : 0;
        char items[max(argslen, envslen) + 1];

        char* args[argslen / 2 + 2];
        int a = 0;
        args[a++] = (char*)cart->command;
        if (argslen > 0) {
            strcpy(items, cart->arguments);
            char* arg = strtok(items, " ");
            while (arg) {
                args[a++] = arg;
                arg = strtok(NULL, " ");
            }
        }
        args[a] = NULL;

        char *envs[envslen / 2 + 1];
        int e = 0;
        if (envslen > 0) {
            strcpy(items, (char*)cart->environment);
            char* env = strtok(items, " ");
            while (env) {
                envs[e++] = env;
                env = strtok(NULL, " ");
            }
        }
        envs[e] = NULL;

        execvpe(cart->command, args, envs);
        exit(0);
    }
    
}


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
        if (cart_context.process_id > 0) {
            cart_execute(cart_context.cart_id);
        }
    }

    if (input_get_button(0, BUTTON_A, BUTTON_PRESSED)) {
        cart_execute(selection_target);
    }

}

static float ease_turn(float a) {
    float a2 = a * a;
    return a2 / (2.0f * (a2 - a) + 1.0f);
}

static float slot_height(float a) {
    return powf(0.5f * (1 + cosf(a)), 10) * 24 - 12;
}

void carousel_draw(pixel_t* volume) {
    float matrix[MAT4_SIZE];

    float smooth = floorf(selection_current) + ease_turn(fmodf(selection_current, 1.0f));

    for (int i = 0; i < cart_count; ++i) {
        float angle =  atan((i - smooth) * 0.5f) * 2; 

        mat4_identity(matrix);
        mat4_apply_translation(matrix, (float[3]){VOXELS_X/2, VOXELS_Y/2, 0});
        mat4_apply_rotation_z(matrix, angle);
        mat4_apply_translation(matrix, (float[3]){40.0f, 4.0f, slot_height(angle)});

        cart_model.surfaces[0].colour = carts[i].colour;
        model_draw(volume, &cart_model, matrix);
    }
}

