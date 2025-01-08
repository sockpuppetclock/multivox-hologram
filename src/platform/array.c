#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "array.h"

void array_initialise(array_t* array, size_t size, size_t capacity) {
    array->size = size;
    array->capacity = capacity;
    array->count = 0;
    array->data = malloc(capacity * size);
    assert(array->data);
}

void array_reserve(array_t* array, size_t capacity) {
    if (capacity > array->capacity) {
        array->capacity = capacity;
        if (array->data) {
            array->data = realloc(array->data, array->capacity * array->size);
            assert(array->data);
        }
    }
    if (!array->data) {
        array->data = malloc(array->capacity * array->size);
        assert(array->data);
        array->count = 0;
    }
}

void array_resize(array_t* array, size_t count) {
    if (!array->data) {
        if (!array->capacity) {
            array->capacity = count > 64 ? count : 64;
        }
        array->count = count;
        array->data = malloc(array->capacity * array->size);
        assert(array->data);
    } else {
        if (count > array->capacity) {
            array->capacity *= 2;
            array->capacity = count > array->capacity ? count : array->capacity;
            array->data = realloc(array->data, array->capacity * array->size);
            assert(array->data);
        }
    }
    array->count = count;
}

void array_clear(array_t* array) {
    array->count = 0;
}

void array_destroy(array_t* array) {
    free(array->data);
    array->data = NULL;
    array->capacity = 0;
    array->count = 0;
}

void array_clear_element(array_t* array, size_t index) {
    memset(array->data + index * array->size, 0, array->size);
}
