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

    int handle;
    uint8_t button_map[16];
    uint8_t axis_map[8];
} controller_t;

static controller_t input_controllers[CONTROLLERS_MAX] = {[0 ... (CONTROLLERS_MAX - 1)].handle = -1};

struct termios termzero;

void reset_terminal_mode() {
    tcsetattr(0, TCSANOW, &termzero);
}

// sets terminal to noncanonical mode, sets nonblocking file reading
void input_set_nonblocking(void) {
    struct termios term;
    
    tcgetattr(STDIN_FILENO, &term);
    memcpy(&termzero, &term, sizeof(term));

    atexit(reset_terminal_mode);

    term.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);

    // set stdin to not block
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

static bool try_open_controller(controller_id_t c) {
    char device[16];
    if (snprintf(device, sizeof(device), "/dev/input/js%d", c) > sizeof(device)) {
        return false;
    }

    controller_t* ctrl = &input_controllers[c];

    int handle = open(device, O_RDONLY | O_NONBLOCK);
    if (handle == -1) {
        return false;
    }

    memset(ctrl, 0, sizeof(*ctrl));
    ctrl->handle = handle;

    uint16_t button_mapping[KEY_MAX - BTN_MISC + 1] = {0};
    if (ioctl(ctrl->handle, JSIOCGBTNMAP, button_mapping) < 0) {
        printf("failed to get button mapping\n");
        for (int i = 0; i < count_of(ctrl->button_map); ++i) {
            ctrl->button_map[i] = i;
        }

    } else {
        memset(ctrl->button_map, 0xff, sizeof(ctrl->button_map));
        for (int i = 0; i < count_of(ctrl->button_map); ++i) {
            switch (button_mapping[i]) {
                case BTN_A:      ctrl->button_map[i] = BUTTON_A; break;
                case BTN_B:      ctrl->button_map[i] = BUTTON_B; break;
                case BTN_X:      ctrl->button_map[i] = BUTTON_X; break;
                case BTN_Y:      ctrl->button_map[i] = BUTTON_Y; break;
                case BTN_Z:      ctrl->button_map[i] = BUTTON_VIEW; break;
                case BTN_TL:     ctrl->button_map[i] = BUTTON_LB; break;
                case BTN_TR:     ctrl->button_map[i] = BUTTON_RB; break;
                case BTN_TL2:    ctrl->button_map[i] = BUTTON_LT; break;
                case BTN_TR2:    ctrl->button_map[i] = BUTTON_RT; break;
                case BTN_SELECT: ctrl->button_map[i] = BUTTON_VIEW; break;
                case BTN_START:  ctrl->button_map[i] = BUTTON_MENU; break;
                case BTN_MODE:   ctrl->button_map[i] = BUTTON_MENU; break;
            }
        }
    }


    uint8_t axis_mapping[ABS_MAX + 1] = {0};
    if (ioctl(ctrl->handle, JSIOCGAXMAP, axis_mapping) < 0) {
        printf("failed to get axis mapping\n");
        for (int i = 0; i < count_of(ctrl->axis_map); ++i) {
            ctrl->axis_map[i] = i;
        }
    } else {
        memset(ctrl->axis_map, 0xff, sizeof(ctrl->axis_map));
        for (int i = 0; i < count_of(ctrl->axis_map); ++i) {
            switch (axis_mapping[i]) {
                case ABS_X:     ctrl->axis_map[i] = AXIS_LS_X; break;
                case ABS_Y:     ctrl->axis_map[i] = AXIS_LS_Y; break;
                case ABS_Z:     ctrl->axis_map[i] = AXIS_LT; break;
                case ABS_RX:    ctrl->axis_map[i] = AXIS_RS_X; break;
                case ABS_RY:    ctrl->axis_map[i] = AXIS_RS_Y; break;
                case ABS_RZ:    ctrl->axis_map[i] = AXIS_RT; break;
                case ABS_HAT0X: ctrl->axis_map[i] = AXIS_D_X; break;
                case ABS_HAT0Y: ctrl->axis_map[i] = AXIS_D_Y; break;
            }
        }
    }

    return true;
}

static void button_press(controller_id_t c, button_t button, bool pressed, uint32_t time) {
    controller_t* ctrl = &input_controllers[c];

    if (pressed) {
        combo_press(c, button, time);
        ctrl->button_states[button] |= BUTTON_PRESSED | BUTTON_HELD;
    } else {
        ctrl->button_states[button] = (ctrl->button_states[button] & ~BUTTON_HELD) | BUTTON_UNPRESSED;
    }
}

void input_update(void) {
    static uint32_t throttle = 0;
    ++throttle;
    
    for (int c = 0; c < CONTROLLERS_MAX; ++c) {
        controller_t* ctrl = &input_controllers[c];

        if (ctrl->handle == -1) {
            if (throttle % (31 + c * 10) == 1) {
                try_open_controller(c);
            }
        }
        
        if (ctrl->handle != -1) {
            for (int i = 0; i < count_of(ctrl->button_states); ++i) {
                ctrl->button_states[i] &= BUTTON_HELD;
            }

            ssize_t events;
            struct js_event event;
            while ((events = read(ctrl->handle, &event, sizeof(event))) > 0) {
                if (event.type == JS_EVENT_BUTTON) {
                    uint8_t button = 0xff;
                    if (event.number < count_of(ctrl->button_map)) {
                        button = ctrl->button_map[event.number];
                    }
                    if (button < BUTTON_COUNT) {
                        button_press(c, button, event.value, event.time);
                        switch (button) {
                            case BUTTON_LT: ctrl->axis_states[AXIS_LT] = event.value ? 1.0f : 0.0f; break;
                            case BUTTON_RT: ctrl->axis_states[AXIS_RT] = event.value ? 1.0f : 0.0f; break;
                        }
                    }
                } else if (event.type == JS_EVENT_AXIS) {
                    uint8_t axis = 0xff;
                    if (event.number < count_of(ctrl->axis_map)) {
                        axis = ctrl->axis_map[event.number];
                    }
                    switch (axis) {
                        case AXIS_LS_X:
                        case AXIS_LS_Y:
                        case AXIS_RS_X:
                        case AXIS_RS_Y: {
                            ctrl->axis_states[axis] = (float)event.value / 32767.0f;
                        } break;
                        
                        case AXIS_LT:
                        case AXIS_RT: {
                            button_t button = (axis == AXIS_LT ? BUTTON_LT : BUTTON_RT);
                            bool pressed = ctrl->axis_states[axis] <= 0.25f && event.value >= 16384;
                            button_press(c, button, pressed, event.time);

                            ctrl->axis_states[axis] = clamp((float)(event.value + 32000) / 64000.0f, 0.0f, 1.0f);
                        } break;

                        case AXIS_D_X:
                        case AXIS_D_Y: {
                            button_t button = (axis == AXIS_D_X ? BUTTON_LEFT : BUTTON_UP) + (event.value > 0);
                            bool pressed = (fabsf(ctrl->axis_states[axis]) <= 0.25f && abs(event.value) >= 16384);
                            button_press(c, button, pressed, event.time);

                            ctrl->axis_states[axis] = (float)event.value / 32767.0f;
                        } break;
                    }
                } else {
                //    printf("event time:%d value:%d type:%d number:%d\n", event.time, event.value, event.type, event.number);
                }
            }
            if (events < 0 && errno != EAGAIN) {
                close(ctrl->handle);
                ctrl->handle = -1;
            }
        }
    }

    static timespec_t timer = {0};
    if (timer_elapsed_ms(&timer) > 1000) {
        // discard any button presses that happened during that suspiciously long pause
        memset(input_controllers->button_states, 0, sizeof(input_controllers->button_states));
    }



}

