#include "memory.h"

#include <stdint.h>

#include "assert.h"

PUBLIC void *byteoffset(void *pointer, ptrdiff_t by) {
    return (char *)pointer + by;
}

PUBLIC linear_allocator_t linear_allocator_init(slice_t data) {
    linear_allocator_t allocator = { data, data.begin };
    return allocator;
}

PUBLIC void linear_allocator_deinit(linear_allocator_t *allocator) {
    assert_debug(allocator->cursor == allocator->data.begin);
}

PUBLIC slice_t linear_allocator_push(linear_allocator_t *allocator, size_t size) {
    void *begin = allocator->cursor;
    void *end = byteoffset(begin, (ptrdiff_t)size);
    assert_debug(end <= allocator->data.end);
    allocator->cursor = end;
    slice_t result = { begin, end };
    return result;
}

PUBLIC slice_t linear_allocator_push_alignment(linear_allocator_t *allocator, size_t alignment) {
    assert_debug((alignment & (alignment - 1)) == 0);
    uintptr_t cursor = (uintptr_t)allocator->cursor;
    uintptr_t aligned = (cursor + (alignment - 1)) & ~(alignment - 1);
    size_t padding = (size_t)(aligned - cursor);
    return linear_allocator_push(allocator, padding);
}

PUBLIC void linear_allocator_pop(linear_allocator_t *allocator, slice_t marker) {
    assert_debug(marker.begin >= allocator->data.begin && marker.end == allocator->cursor);
    allocator->cursor = marker.begin;
}

PUBLIC void linear_allocator_pop_move(linear_allocator_t *allocator, slice_t from, slice_t to) {
    assert_debug(to.begin <= from.begin);
    assert_debug(from.end == allocator->cursor);
    ptrdiff_t size = SLICE_BYTESIZE(from);
    __builtin_memmove(to.begin, from.begin, (size_t)size);
    allocator->cursor = byteoffset(to.begin, size);
}

// Grows `allocator` by `size` bytes and opens a `size`-byte gap at
// `position` by sliding everything between `position` and the old cursor up
// to make room. Callers holding slices/pointers into that shifted region
// must rebase them by `size` themselves -- this only moves the bytes.
PUBLIC void linear_allocator_insert(linear_allocator_t *allocator, void *at, size_t size) {
    assert_debug(at >= allocator->data.begin && at <= allocator->cursor);
    void *old_cursor = allocator->cursor;
    linear_allocator_push(allocator, size);
    ptrdiff_t tail_size = bytesize(at, old_cursor);
    assert_debug(tail_size >= 0);
    __builtin_memmove(byteoffset(at, (ptrdiff_t)size), at, (size_t)tail_size);
}

PUBLIC void *slice_at(slice_t s, size_t index, size_t alignment) {
    assert_debug((alignment & (alignment - 1)) == 0);
    void *result = byteoffset(s.begin, (ptrdiff_t)index);
    assert_debug(result <= s.end);
    assert_debug(result >= s.begin);
    assert_debug(((uintptr_t)result & (alignment - 1)) == 0);
    return result;
}

PUBLIC slice_t slice_advance(slice_t s, size_t by) {
    void *begin = byteoffset(s.begin, (ptrdiff_t)by);
    assert_debug(begin <= s.end);
    slice_t result = { begin, s.end };
    return result;
}

PUBLIC slice_t slice_shift(slice_t s, ptrdiff_t by) {
    slice_t result = { byteoffset(s.begin, by), byteoffset(s.end, by) };
    return result;
}

PUBLIC ptrdiff_t bytesize(void *begin, void *end) {
    return (char *)end - (char *)begin;
}
