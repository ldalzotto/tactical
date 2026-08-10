#pragma once

#include <stdbool.h>
#include "../lib/memory.h"

typedef struct {
    int x, y;
} position_t;

SLICE_DEFINE(position_t);

position_t position_add(position_t a, position_t b);
position_t position_sub(position_t a, position_t b);
bool position_equals(position_t a, position_t b);

// Orthogonal unit offsets: up, right, down, left.
static const position_t __POSITION_DIRECTIONS[4] = {
    { 0, -1 }, // up
    { 1, 0 },  // right
    { 0, 1 },  // down
    { -1, 0 }, // left
};

#define POSITION_DIRECTIONS (slice_position_t) { \
        (position_t*)__POSITION_DIRECTIONS, \
        typeoffset((position_t*)__POSITION_DIRECTIONS, sizeof(__POSITION_DIRECTIONS)/sizeof(__POSITION_DIRECTIONS[0])), \
    }
