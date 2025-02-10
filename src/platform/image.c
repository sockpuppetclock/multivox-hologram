#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "image.h"

#include "rammel.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

image_t* image_load(const char* filename) {
    image_t* image = NULL;

    int w = 0, h = 0, c = 0;
    uint8_t* data = stbi_load(filename, &w, &h, &c, STBI_default);

    if (data) {
        if (w > 0 && h > 0) {
            image = malloc(sizeof(image_t));
            memset(image, 0, sizeof(image_t));
        
            image->width = w;
            image->height = h;

            image->masked = false;
            image->key = HEXPIX(200000);

            int size = image->width * image->height;
            image->data = malloc(sizeof(pixel_t) * size);

            int r, g, b, a;
            switch (c) {
                case 2: r = g = b = 0; a = 1; break;
                case 3: r = 0; g = 1; b = 2; a = 0; break;
                case 4: r = 0; g = 1; b = 2; a = 3; break;
                default: r = g = b = a = 0; break;
            }

            if (a) {
                bool key_clash = false;
                for (int i = 0; i < size; ++i) {
                    uint8_t* p = &data[i*c];
                    pixel_t colour = RGBPIX(p[r], p[g], p[b]);
                    if (p[a] < 128) {
                        image->masked = true;
                        colour = image->key;
                    } else if (colour == image->key) {
                        key_clash = true;
                    }
                    image->data[i] = colour;
                }
                if (image->masked && key_clash) {
                    for (int i = 0; i < size; ++i) {
                        uint8_t* p = &data[i*c];
                        if (image->data[i] == image->key && p[a] >= 128) {
                            image->data[i] ^= HEXPIX(200000);
                        }
                    }
                }
            } else {
                for (int i = 0; i < size; ++i) {
                    uint8_t* p = &data[i*c];
                    image->data[i] = RGBPIX(p[r], p[g], p[b]);
                }
            }
        }
        stbi_image_free(data);
    }

    return image;
}

pixel_t image_sample(image_t* image, const float* uv, bool* masked) {
    if (!image->data || image->width <= 0 || image->height <= 0) {
        return 0;
    }

    int x = modulo((int)floor(uv[0] * image->width), image->width);
    int y = modulo((int)floor(uv[1] * image->height), image->height);

    y = (image->height - 1) - y;

    pixel_t colour = image->data[x + image->width * y];

    if (masked) {
        *masked = image->masked && colour == image->key;
    }

    return colour;
}

void image_free(image_t* image) {
    if (image) {
        free(image->data);
        free(image);
    }
}
