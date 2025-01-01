#ifndef _IMAGE_H_
#define _IMAGE_H_

#include "voxel.h"

typedef struct image_s {
    pixel_t* data;
    int width, height;

    uint8_t masked;
    pixel_t key;
} image_t;

image_t* image_load(const char* filename);
pixel_t image_sample(image_t* image, const float* uv, bool* masked);
void image_free(image_t* image);


#endif
