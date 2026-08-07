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
slice_t linear_allocator_push_alignment(linear_allocator_t *allocator, size_t alignment);
void linear_allocator_pop(linear_allocator_t *allocator, slice_t marker);

void *slice_at(slice_t s, size_t index, size_t alignment);
slice_t slice_advance(slice_t s, size_t by);

#define SLICE_DEFINE(type) \
    typedef union { slice_t slice; struct { type *begin; type *end; }; } slice_##type

#define SLICE_AT(s, index) \
    (*(__typeof__((s).begin))slice_at((s).slice, (size_t)(index) * sizeof(*(s).begin), _Alignof(__typeof__(*(s).begin))))

#define SLICE_ADVANCE(s, by) \
    ((__typeof__(s)){ .slice = slice_advance((s).slice, (size_t)(by) * sizeof(*(s).begin)) })
