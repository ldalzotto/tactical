#include "memory.h"

#include <stdint.h>

#include "assert.h"

void *byteoffset(void *pointer, ptrdiff_t by) {
    return (char *)pointer + by;
}

linear_allocator_t linear_allocator_init(slice_t data) {
    linear_allocator_t allocator = { data, data.begin };
    return allocator;
}

void linear_allocator_deinit(linear_allocator_t *allocator) {
    assert_debug(allocator->cursor == allocator->data.begin);
}

slice_t linear_allocator_push(linear_allocator_t *allocator, size_t size) {
    void *begin = allocator->cursor;
    void *end = byteoffset(begin, (ptrdiff_t)size);
    assert_debug(end <= allocator->data.end);
    allocator->cursor = end;
    slice_t result = { begin, end };
    return result;
}

slice_t linear_allocator_push_alignment(linear_allocator_t *allocator, size_t alignment) {
    assert_debug((alignment & (alignment - 1)) == 0);
    uintptr_t cursor = (uintptr_t)allocator->cursor;
    uintptr_t aligned = (cursor + (alignment - 1)) & ~(alignment - 1);
    size_t padding = (size_t)(aligned - cursor);
    return linear_allocator_push(allocator, padding);
}

void linear_allocator_pop(linear_allocator_t *allocator, slice_t marker) {
    assert_debug(marker.begin >= allocator->data.begin && marker.end == allocator->cursor);
    allocator->cursor = marker.begin;
}

void linear_allocator_pop_move(linear_allocator_t *allocator, slice_t from, slice_t to) {
    assert_debug(to.begin <= from.begin);
    assert_debug(from.end == allocator->cursor);
    ptrdiff_t size = SLICE_BYTESIZE(from);
    __builtin_memmove(to.begin, from.begin, (size_t)size);
    allocator->cursor = byteoffset(to.begin, size);
}

void *slice_at(slice_t s, size_t index, size_t alignment) {
    assert_debug((alignment & (alignment - 1)) == 0);
    void *result = byteoffset(s.begin, (ptrdiff_t)index);
    assert_debug(result <= s.end);
    assert_debug(((uintptr_t)result & (alignment - 1)) == 0);
    return result;
}

slice_t slice_advance(slice_t s, size_t by) {
    void *begin = byteoffset(s.begin, (ptrdiff_t)by);
    assert_debug(begin <= s.end);
    slice_t result = { begin, s.end };
    return result;
}

ptrdiff_t bytesize(void *begin, void *end) {
    return (char *)end - (char *)begin;
}
