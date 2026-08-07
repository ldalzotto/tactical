#include "memory.h"

#include <stdint.h>

#include "assert.h"

linear_allocator_t linear_allocator_init(slice_t data) {
    linear_allocator_t allocator = { data, data.begin };
    return allocator;
}

void linear_allocator_deinit(linear_allocator_t *allocator) {
    assert(allocator->cursor == allocator->data.begin);
}

slice_t linear_allocator_push(linear_allocator_t *allocator, size_t size) {
    void *begin = allocator->cursor;
    void *end = (char *)begin + size;
    assert(end <= allocator->data.end);
    allocator->cursor = end;
    slice_t result = { begin, end };
    return result;
}

slice_t linear_allocator_push_alignment(linear_allocator_t *allocator, size_t alignment) {
    assert((alignment & (alignment - 1)) == 0);
    uintptr_t cursor = (uintptr_t)allocator->cursor;
    uintptr_t aligned = (cursor + (alignment - 1)) & ~(alignment - 1);
    size_t padding = (size_t)(aligned - cursor);
    return linear_allocator_push(allocator, padding);
}

void linear_allocator_pop(linear_allocator_t *allocator, slice_t marker) {
    assert(marker.begin >= allocator->data.begin && marker.end == allocator->cursor);
    allocator->cursor = marker.begin;
}
