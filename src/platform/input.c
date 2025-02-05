#include "input.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <linux/joystick.h>
#include <errno.h>
#include <math.h>
#include <time.h>

#include "rammel.h"
#include "timer.h"

typedef struct {
    uint8_t button_states[BUTTON_COUNT];
    float axis_states[AXIS_COUNT];

    uint8_t combo_buffer[16];
    uint32_t combo_cursor;
    uint32_t combo_last_time;
} controller_t;

static int input_handles[CONTROLLERS_MAX] = {[0 ... (CONTROLLERS_MAX - 1)] = -1};
static controller_t input_controllers[CONTROLLERS_MAX];

struct termios termzero;

void reset_terminal_mode() {
    tcsetattr(0, TCSANOW, &termzero);
}

void input_set_nonblocking(void) {
    struct termios term;
    
    tcgetattr(STDIN_FILENO, &term);
    memcpy(&termzero, &term, sizeof(term));

    atexit(reset_terminal_mode);

    term.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);

    int fc = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, fc | O_NONBLOCK);
}

bool input_get_button(controller_id_t controller, button_t button, button_event_t event) {
    if ((uint)controller >= count_of(input_controllers) ||
        (uint)button >= count_of(input_controllers[controller].button_states)) {
        return false;
    }

    return (input_controllers[controller].button_states[button] & event) != 0;
}

float input_get_axis(controller_id_t controller, axis_t axis) {
    if ((uint)controller >= count_of(input_controllers) ||
        (uint)axis >= count_of(input_controllers[controller].axis_states)) {
        return 0;
    }

    return input_controllers[controller].axis_states[axis];
}

static void combo_press(controller_id_t controller, uint8_t button, uint32_t time) {
    controller_t* ctrl = &input_controllers[controller];
    if (time - ctrl->combo_last_time >= 1000) {
        ctrl->combo_buffer[ctrl->combo_cursor] = 0;
        ctrl->combo_cursor = (ctrl->combo_cursor + 1) % count_of(ctrl->combo_buffer);
    }

    ctrl->combo_buffer[ctrl->combo_cursor] = button;
    ctrl->combo_cursor = (ctrl->combo_cursor + 1) % count_of(ctrl->combo_buffer);
    
    ctrl->combo_last_time = time;
}

bool input_get_combo(controller_id_t controller, const uint8_t* combo, uint8_t combo_length) {
    if ((uint)controller >= count_of(input_controllers)) {
        return false;
    }

    controller_t* ctrl = &input_controllers[controller];
    uint32_t c = modulo(ctrl->combo_cursor - combo_length, count_of(ctrl->combo_buffer));
    for (int i = 0; i < combo_length; ++i) {
        if (ctrl->combo_buffer[c] != combo[i]) {
            return false;
        }
        c = (c + 1) % count_of(ctrl->combo_buffer);
    }
    return true;
}

void input_update(void) {
    static uint32_t throttle = 0;
    ++throttle;
    
    for (int c = 0; c < CONTROLLERS_MAX; ++c) {
        if (input_handles[c] == -1) {
            if (throttle % (31 + c * 10) == 1) {
                char device[16];
                if (snprintf(device, sizeof(device), "/dev/input/js%d", c) <= sizeof(device)) {
                    input_handles[c] = open(device, O_RDONLY | O_NONBLOCK);
                    if (input_handles[c] != -1) {
                        memset(&input_controllers[c], 0, sizeof(input_controllers[c]));
                    }
                }
            }
        }
        
        if (input_handles[c] != -1) {
            controller_t* ctrl = &input_controllers[c];

            for (int i = 0; i < count_of(ctrl->button_states); ++i) {
                ctrl->button_states[i] &= BUTTON_HELD;
            }

            ssize_t events;
            struct js_event event;
            while ((events = read(input_handles[c], &event, sizeof(event))) > 0) {
                if (event.type == JS_EVENT_BUTTON) {
                    if (event.number < BUTTON_COUNT) {
                        if (event.value) {
                            combo_press(c, event.number, event.time);
                            ctrl->button_states[event.number] |= BUTTON_PRESSED | BUTTON_HELD;
                        } else {
                            ctrl->button_states[event.number] = (ctrl->button_states[event.number] & ~BUTTON_HELD) | BUTTON_UNPRESSED;
                        }
                    }
                } else if (event.type == JS_EVENT_AXIS) {
                    switch (event.number) {
                        case AXIS_LS_X:
                        case AXIS_LS_Y:
                        case AXIS_RS_X:
                        case AXIS_RS_Y:
                            ctrl->axis_states[event.number] = (float)event.value / 32767.0f;
                            break;
                        
                        case AXIS_LT:
                        case AXIS_RT:
                            ctrl->axis_states[event.number] = clamp((float)(event.value + 32000) / 64000.0f, 0.0f, 1.0f);
                            break;

                        case AXIS_D_X:
                        case AXIS_D_Y:
                            button_t button = (event.number == AXIS_D_X ? BUTTON_LEFT : BUTTON_UP) + (event.value > 0);
                            
                            if (fabsf(ctrl->axis_states[event.number]) <= 0.25f && abs(event.value) >= 16384) {
                                combo_press(c, button, event.time);
                                ctrl->button_states[button] = BUTTON_PRESSED | BUTTON_HELD;

                            } else if (fabsf(ctrl->axis_states[event.number]) >= 0.5f && abs(event.value) <= 8192) {
                                ctrl->button_states[button] = BUTTON_UNPRESSED;
                            }

                            ctrl->axis_states[event.number] = (float)event.value / 32767.0f;
                            break;
                    }
                } else {
                //    printf("event time:%d value:%d type:%d number:%d\n", event.time, event.value, event.type, event.number);
                }
            }
            if (events < 0 && errno != EAGAIN) {
                close(input_handles[c]);
                input_handles[c] = -1;
            }
        }
    }

    static clock_t time_prev = {0};
    clock_t time_curr = clock();
    clock_t elapsed = time_curr - time_prev;
    time_prev = time_curr;
    
    if (elapsed > 1000) {
        // discard everything that happened during that suspiciously long pause
        memset(input_controllers, 0, sizeof(input_controllers));
    }



}

