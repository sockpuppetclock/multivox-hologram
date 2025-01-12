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

static uint8_t button_states[BUTTON_COUNT];
static float axis_states[AXIS_COUNT];

static uint8_t combo_buffer[16];
static uint32_t combo_cursor;
static uint32_t combo_last_time;

static int32_t diff_timespec_ms(const struct timespec* to, const struct timespec* from) {
    struct timespec diff;

    if ((to->tv_nsec - from->tv_nsec) < 0) {
        diff.tv_sec = to->tv_sec - from->tv_sec - 1;
        diff.tv_nsec = 1000000000 + to->tv_nsec - from->tv_nsec;
    } else {
        diff.tv_sec = to->tv_sec - from->tv_sec;
        diff.tv_nsec = to->tv_nsec - from->tv_nsec;
    }

    return (diff.tv_sec * 1000) + (diff.tv_nsec / 1000000);
}

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

static void combo_press(uint8_t button, uint32_t time) {
    if (time - combo_last_time >= 1000) {
        combo_buffer[combo_cursor] = 0;
        combo_cursor = (combo_cursor + 1) % count_of(combo_buffer);
    }

    combo_buffer[combo_cursor] = button;
    combo_cursor = (combo_cursor + 1) % count_of(combo_buffer);
    
    combo_last_time = time;
}

bool input_get_combo(const uint8_t* combo, uint8_t combo_length) {
    uint32_t c = modulo(combo_cursor - combo_length, count_of(combo_buffer));
    for (int i = 0; i < combo_length; ++i) {
        if (combo_buffer[c] != combo[i]) {
            return false;
        }
        c = (c + 1) % count_of(combo_buffer);
    }
    return true;
}

static void clear_input(void) {
    memset(button_states, 0, sizeof(button_states));
    memset(combo_buffer, 0, sizeof(combo_buffer));
    combo_cursor = 0;
}

void input_update(void) {
    static int js = -1;

    for (int i = 0; i < count_of(button_states); ++i) {
        button_states[i] &= BUTTON_HELD;
    }

    if (js == -1) {
        js = open("/dev/input/js0", O_RDONLY | O_NONBLOCK);
        if (js != -1) {
            clear_input();
        }
    }
    
    if (js != -1) {
        ssize_t events;
        struct js_event event;
        while ((events = read(js, &event, sizeof(event))) > 0) {
            if (event.type == JS_EVENT_BUTTON) {
                if (event.number < BUTTON_COUNT) {
                    if (event.value) {
                        combo_press(event.number, event.time);
                        button_states[event.number] |= BUTTON_PRESSED | BUTTON_HELD;
                    } else {
                        button_states[event.number] = (button_states[event.number] & ~BUTTON_HELD) | BUTTON_UNPRESSED;
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
                        button_t button = (event.number == AXIS_D_X ? BUTTON_LEFT : BUTTON_UP) + (event.value > 0);
                        
                        if (fabsf(axis_states[event.number]) <= 0.25f && abs(event.value) >= 16384) {
                            combo_press(button, event.time);
                            button_states[button] = BUTTON_PRESSED | BUTTON_HELD;

                        } else if (fabsf(axis_states[event.number]) >= 0.5f && abs(event.value) <= 8192) {
                            button_states[button] = BUTTON_UNPRESSED;
                        }

                        axis_states[event.number] = (float)event.value / 32767.0f;
                        break;
                }
            } else {
            //    printf("event time:%d value:%d type:%d number:%d\n", event.time, event.value, event.type, event.number);
            }
        }
        if (events < 0 && errno != EAGAIN) {
            close(js);
            js = -1;
        }
    }


    static struct timespec time_prev = {0};
    struct timespec time_curr;
    clock_gettime(CLOCK_REALTIME, &time_curr);
    int32_t elapsed = diff_timespec_ms(&time_curr, &time_prev);
    time_prev = time_curr;
    
    if (elapsed > 1000) {
        // discard everything that happened during that suspiciously long pause
        clear_input();
    }



}

