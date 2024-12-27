#ifndef _INPUT_H_
#define _INPUT_H_

typedef enum {
    AXIS_LS_X,
    AXIS_LS_Y,
    AXIS_LT,
    AXIS_RS_X,
    AXIS_RS_Y,
    AXIS_RT,
    AXIS_D_X,
    AXIS_D_Y
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
    BUTTON_DOWN
} button_t;

void input_set_nonblocking(void);

#endif
