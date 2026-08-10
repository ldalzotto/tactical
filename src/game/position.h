#pragma once

#include <stdbool.h>

typedef struct {
    int x, y;
} position_t;

position_t position_add(position_t a, position_t b);
position_t position_sub(position_t a, position_t b);
bool position_equals(position_t a, position_t b);

// Orthogonal unit offsets: up, right, down, left.
static const position_t POSITION_DIRECTIONS[4] = {
    { 0, -1 }, // up
    { 1, 0 },  // right
    { 0, 1 },  // down
    { -1, 0 }, // left
};
