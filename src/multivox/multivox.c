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
                    cart_grab_shot(cart_context.cart, voxel_buffer_get(VOXEL_BUFFER_FRONT));
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

            cart_grab_shot(cart_context.cart, voxel_buffer_get(VOXEL_BUFFER_FRONT));
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




