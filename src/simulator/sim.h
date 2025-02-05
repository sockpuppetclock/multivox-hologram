#ifndef _SIM_H_
#define _SIM_H_

#include <stdbool.h>

bool sim_init(int argc, char** argv);
void sim_resize(int w, int h);
void sim_drag(int button, float dx, float dy);
void sim_zoom(float d);
void sim_draw(void);

#endif
