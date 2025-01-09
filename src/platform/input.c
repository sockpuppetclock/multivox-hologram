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

#include "rammel.h"

static uint8_t button_states[BUTTON_COUNT] = {0};
static float axis_states[AXIS_COUNT] = {0};

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
    if ((uint)button >= count_of(button_states)) {
        return false;
    }

    return (button_states[button] & event) != 0;
}

float input_get_axis(controller_id_t controller, axis_t axis) {
    if ((uint)axis >= count_of(axis_states)) {
        return 0;
    }

    return axis_states[axis];
}

void input_update(void) {
    static int js = -1;

    for (int i = 0; i < count_of(button_states); ++i) {
        button_states[i] &= BUTTON_HELD;
    }

    if (js == -1) {
        js = open("/dev/input/js0", O_RDONLY | O_NONBLOCK);
    } else {
        ssize_t events;
        struct js_event event;
        while ((events = read(js, &event, sizeof(event))) > 0) {
            if (event.type == JS_EVENT_BUTTON) {
                if (event.number < BUTTON_COUNT) {
                    if (event.value) {
                        button_states[event.number] = BUTTON_PRESSED | BUTTON_HELD;
                    } else {
                        button_states[event.number] = BUTTON_UNPRESSED;
                    }

                }
            } else if (event.type == JS_EVENT_AXIS) {
                switch (event.number) {
                    case AXIS_LS_X:
                    case AXIS_LS_Y:
                    case AXIS_RS_X:
                    case AXIS_RS_Y:
                        axis_states[event.number] = (float)event.value / 32767.0f;
                        break;
                    
                    case AXIS_LT:
                    case AXIS_RT:
                        axis_states[event.number] = clamp((float)(event.value + 32000) / 64000.0f, 0.0f, 1.0f);
                        break;

                    case AXIS_D_X:
                    case AXIS_D_Y:
                        button_t button = event.number == AXIS_D_X ? BUTTON_LEFT : BUTTON_UP;
                        if (fabsf(axis_states[event.number]) <= 0.25f && abs(event.value) >= 16384) {
                            button_states[button + (event.value > 0)] = BUTTON_PRESSED | BUTTON_HELD;
                        } else if (fabsf(axis_states[event.number]) >= 0.5f && abs(event.value) <= 8192) {
                            button_states[button + (event.value > 0)] = BUTTON_UNPRESSED;
                        }
                        axis_states[event.number] = (float)event.value / 32767.0f;
                        break;
                }
            }
        }
        if (events < 0 && errno != EAGAIN) {
            close(js);
            js = -1;
        }
    }
}

