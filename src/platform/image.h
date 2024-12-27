#ifndef _IMAGE_H_
#define _IMAGE_H_

#include <stddef.h>
#include "gadget.h"

typedef struct {
    pixel_t* data;
    int width, height;
} image_t;

image_t* image_load(const char* filename);
pixel_t image_sample(image_t* image, const float* uv);
void image_free(image_t* image);


#endif
