#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "image.h"

#include "graphics.h"
#include "mathc.h"
#include "rammel.h"
#include "array.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

image_t* image_load(const char* filename) {
    image_t* image = NULL;

    int w = 0, h = 0, c = 0;
    uint8_t* data = stbi_load(filename, &w, &h, &c, STBI_rgb);
    if (data) {
        //printf("%s %dx%d\n", filename, w, h);
        if (w > 0 && h > 0) {
            image = malloc(sizeof(image_t));
            memset(image, 0, sizeof(image_t));

            image->width = w;
            image->height = h;
            int size = image->width * image->height;
            image->data = malloc(sizeof(pixel_t) * size);

            for (int i = 0; i < size; ++i) {
                uint8_t* p = &data[i * c];
                image->data[i] = RGBPIX(p[0], p[c > 1], p[(c > 2) << 1]);
            }
        }
        stbi_image_free(data);
    }

    return image;
}

pixel_t image_sample(image_t* image, const float* uv) {
    if (!image->data || image->width <= 0 || image->height <= 0) {
        return 0;
    }

    int x = modulo((int)floor(uv[0] * image->width), image->width);
    int y = modulo((int)floor(uv[1] * image->height), image->height);

    y = (image->height - 1) - y;

    return image->data[x + image->width * y];
}

void image_free(image_t* image) {
    if (image) {
        free(image->data);
        free(image);
    }
}
