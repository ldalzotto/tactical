#pragma once

#include <stddef.h>

typedef struct {
    void *begin;
    void *end;
} slice_t;

typedef struct {
    slice_t data;
    void *cursor;
} linear_allocator_t;

linear_allocator_t linear_allocator_init(slice_t data);
void linear_allocator_deinit(linear_allocator_t *allocator);
slice_t linear_allocator_push(linear_allocator_t *allocator, size_t size);
void linear_allocator_pop(linear_allocator_t *allocator, slice_t marker);
