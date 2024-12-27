#ifndef _ARRAY_H_
#define _ARRAY_H_

#include <stddef.h>

typedef struct {
    size_t size;
    size_t capacity;
    size_t count;
    void* data;
} array_t;

void array_initialise(array_t* array, size_t size, size_t capacity);
void array_reserve(array_t* array, size_t capacity);
void array_resize(array_t* array, size_t count);
void array_clear(array_t* array);
void array_destroy(array_t* array);

void array_clear_element(array_t* array, size_t index);

#endif
