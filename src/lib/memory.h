#pragma once

#include "linkage.h"

#include <stddef.h>

typedef struct {
    void *begin;
    void *end;
} slice_t;

typedef struct {
    slice_t data;
    void *cursor;
} linear_allocator_t;

PUBLIC linear_allocator_t linear_allocator_init(slice_t data);
PUBLIC void linear_allocator_deinit(linear_allocator_t *allocator);
PUBLIC slice_t linear_allocator_push(linear_allocator_t *allocator, size_t size);
PUBLIC slice_t linear_allocator_push_alignment(linear_allocator_t *allocator, size_t alignment);
PUBLIC void linear_allocator_pop(linear_allocator_t *allocator, slice_t marker);
PUBLIC void linear_allocator_pop_move(linear_allocator_t *allocator, slice_t from, slice_t to);
PUBLIC void linear_allocator_insert(linear_allocator_t *allocator, void *position, size_t size);

#define LINEAR_ALLOCATOR_PUSH(allocator, witness, count) \
    ((typeof(witness)){ .slice = linear_allocator_push((allocator), (size_t)(count) * sizeof(*(witness).begin)) })

#define LINEAR_ALLOCATOR_PUSH_ALIGNMENT(allocator, witness) \
    linear_allocator_push_alignment((allocator), _Alignof(typeof(*(witness).begin)))

#define LINEAR_ALLOCATOR_POP(allocator, typed_slice) \
    linear_allocator_pop((allocator), (typed_slice).slice)

#define LINEAR_ALLOCATOR_POP_MOVE(allocator, from, to) \
    linear_allocator_pop_move((allocator), (from).slice, (to).slice)

PUBLIC void *byteoffset(void *pointer, ptrdiff_t by);
#define typeoffset(pointer, by) (typeof(*pointer)*)byteoffset(pointer, by * sizeof(typeof(*pointer)))

PUBLIC void *slice_at(slice_t s, size_t index, size_t alignment);
PUBLIC slice_t slice_advance(slice_t s, size_t by);
PUBLIC slice_t slice_shift(slice_t s, ptrdiff_t by);
PUBLIC ptrdiff_t bytesize(void *begin, void *end);
#define typesize(begin, end) bytesize(begin, end) / sizeof(*begin)

#define SLICE_DEFINE(type) \
    typedef union { slice_t slice; struct { type *begin; type *end; }; } slice_##type

#define SLICE_AT(s, index) \
    (*(typeof((s).begin))slice_at((s).slice, (size_t)(index) * sizeof(*(s).begin), _Alignof(typeof(*(s).begin))))

#define SLICE_DEREF(s) SLICE_AT(s, 0)

#define SLICE_ADVANCE(s, by) \
    ((typeof(s)){ .slice = slice_advance((s).slice, (size_t)(by) * sizeof(*(s).begin)) })

#define SLICE_FOREACH(s, var) typeof(s) var = s; SLICE_BYTESIZE(var) != 0; var = SLICE_ADVANCE(var, 1)

#define SLICE_BYTESIZE(s) bytesize((s).begin, (s).end)
#define SLICE_TYPESIZE(s) typesize((s).begin, (s).end)

#define STR(litteral) (slice_t){.begin = litteral, .end = byteoffset(litteral, sizeof(litteral) - 1)}
#ifdef APP_UNITY_BUILD
#include "memory.c"
#endif
