#ifndef _INPUT_H_
#define _INPUT_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    AXIS_LS_X,
    AXIS_LS_Y,
    AXIS_LT,
    AXIS_RS_X,
    AXIS_RS_Y,
    AXIS_RT,
    AXIS_D_X,
    AXIS_D_Y,
    AXIS_COUNT
} axis_t;

typedef enum {
    BUTTON_A,
    BUTTON_B,
    BUTTON_X,
    BUTTON_Y,
    BUTTON_LB,
    BUTTON_RB,
    BUTTON_VIEW,
    BUTTON_MENU,
    BUTTON_LEFT,
    BUTTON_RIGHT,
    BUTTON_UP,
    BUTTON_DOWN,
    BUTTON_COUNT
} button_t;

typedef enum {
    BUTTON_UNPRESSED = 1,
    BUTTON_PRESSED = 2,
    BUTTON_HELD = 4
} button_event_t;

typedef uint8_t controller_id_t;

void input_set_nonblocking(void);
bool input_get_button(controller_id_t controller, button_t button, button_event_t event);
float input_get_axis(controller_id_t controller, axis_t axis);
bool input_get_combo(const uint8_t* combo, uint8_t combo_length);
void input_update(void);

#endif
