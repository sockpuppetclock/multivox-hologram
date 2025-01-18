#define _GNU_SOURCE
#include "multivox.h"

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
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "array.h"
#include "mathc.h"
#include "rammel.h"
#include "input.h"
#include "voxel.h"
#include "graphics.h"
#include "model.h"
#include "timer.h"
#include "carousel.h"


void main_init(void) {
    timer_init();
    carousel_init();
}

void main_update(float dt) {    
    input_update();
    carousel_update(dt);
}

void main_draw(pixel_t* volume) {
    carousel_draw(volume);
}

int main(int argc, char** argv) {

    if (!voxel_buffer_map()) {
        printf("waiting for voxel buffer\n");
        do {
            sleep(1);
        } while (!voxel_buffer_map());
    }

    input_set_nonblocking();

    main_init();

    for (int ch = 0; ch != 27; ch = getchar()) {
        pixel_t* volume = voxel_buffer_get(VOXEL_BUFFER_BACK);
        voxel_buffer_clear(volume);

        timer_tick();

        main_update((float)timer_delta_time * 0.001f);
        main_draw(volume);

        voxel_buffer_swap();

        timer_sleep_until(TIMER_SINCE_TICK, 30);
    }

    voxel_buffer_unmap();

    return 0;
}




typedef struct {
    cart_t* cart;
    pid_t process_id;

} cart_context_t;

static cart_context_t cart_context = {.process_id = -1};

static void grab_volume(cart_t* cart) {
    if (!cart->voxel_shot[0]) {
        cart->voxel_shot[0] = malloc(VOXELS_COUNT * sizeof(pixel_t));
        memset(cart->voxel_shot[0], 0, VOXELS_COUNT * sizeof(pixel_t));
    }

    pixel_t* volume = voxel_buffer_get(VOXEL_BUFFER_FRONT);
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

static bool background_process() {
    input_update();

    if (input_get_button(0, BUTTON_VIEW, BUTTON_PRESSED)) {
        return false;
    }

    usleep(100000);

    return true;
}

static cart_action_t background_loop(pid_t pid) {

    while (true) {
        int status;
        pid_t result = waitpid(pid, &status, WNOHANG);

        if (result == 0) {
            if (!background_process()) {
                //printf("suspend\n");
                if (kill(cart_context.process_id, SIGSTOP) == 0) {
                    grab_volume(cart_context.cart);
                    return CART_ACTION_PAUSE;

                } else {
                    perror("suspend");
                }
            }

        } else if (result == -1) {
            perror("waitpid");
            return CART_ACTION_FAIL;
            
        } else {
            if (WIFEXITED(status)) {
                printf("Child process exited with status %d\n", WEXITSTATUS(status));

            } else if (WIFSIGNALED(status)) {
                printf("Child process killed by signal %d\n", WTERMSIG(status));

            }

            grab_volume(cart_context.cart);
            return CART_ACTION_EJECT;
        }
    }
}

cart_action_t multivox_cart_resume(void) {
    if (cart_context.process_id > 0 && cart_context.cart) {
        return multivox_cart_execute(cart_context.cart);
    }

    return CART_ACTION_NONE;
}

cart_action_t multivox_cart_execute(cart_t* cart) {
    static uint32_t nospam = 0;
    if ((timer_frame_count - nospam) < 10) {
        printf("nospam\n");
        return CART_ACTION_FAIL;
    }
    nospam = timer_frame_count;

    if (!cart || !cart->command) {
        return CART_ACTION_FAIL;
    }

    #define IF_STRING(str) ((str) ? (str) : "")
    printf("cartecute %s %s %s\n", IF_STRING(cart->command), IF_STRING(cart->workingdir), IF_STRING(cart->environment));

    if (cart_context.process_id > 0) {
        if (cart_context.cart == cart) {
            //printf("resume\n");
            if (kill(cart_context.process_id, SIGCONT) == 0) {
                return background_loop(cart_context.process_id);
            }
            perror("process");
        }

        //printf("kill\n");
        kill(cart_context.process_id, SIGKILL);
    }

    pid_t pid = fork();
    cart_context.process_id = pid;
    cart_context.cart = cart;

    printf("pid %d\n", pid);

    if (pid == -1) {
        perror("fork");
        return CART_ACTION_FAIL;
    }

    if (pid == 0) {
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
    
    // wait until we get regain focus
    return background_loop(pid);
}




