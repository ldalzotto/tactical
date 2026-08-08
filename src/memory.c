#include "memory.h"

#include <stdint.h>

#include "assert.h"
#include "memory_debug.h"

void *byteoffset(void *pointer, ptrdiff_t by) {
    return (char *)pointer + by;
}

linear_allocator_t linear_allocator_init(slice_t data) {
    linear_allocator_t allocator = { data, data.begin };
    return allocator;
}

void linear_allocator_deinit(linear_allocator_t *allocator) {
    assert(allocator->cursor == allocator->data.begin);
}

slice_t linear_allocator_push(linear_allocator_t *allocator, size_t size) {
    void *begin = allocator->cursor;
    void *end = byteoffset(begin, (ptrdiff_t)size);
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
    assert(marker.debug_id == 0);
    allocator->cursor = marker.begin;
}

void *slice_at(slice_t s, size_t index, size_t alignment) {
    assert((alignment & (alignment - 1)) == 0);
    void *result = byteoffset(s.begin, (ptrdiff_t)index);
    assert(result < s.end); // We are going to deref the pointer
    assert(((uintptr_t)result & (alignment - 1)) == 0);
#ifndef NDEBUG
    memory_debug_check_access(s, result);
#endif
    return result;
}

slice_t slice_advance(slice_t s, size_t by) {
    void *begin = byteoffset(s.begin, (ptrdiff_t)by);
    assert(begin <= s.end); // We are allowed to advance to the end
    slice_t result = s;
    result.begin = begin;
    return result;
}
