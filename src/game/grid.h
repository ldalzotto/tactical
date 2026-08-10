#pragma once

#include <stdbool.h>

#include "../lib/memory.h"
#include "position.h"

typedef struct tile tile_t;

SLICE_DEFINE(tile_t);

typedef struct {
    int width, height;
    slice_tile_t tiles;
} grid_t;

grid_t grid_init(linear_allocator_t *allocator, int width, int height);
void grid_deinit(linear_allocator_t *allocator, grid_t grid);
bool grid_in_bounds(grid_t grid, position_t position);
void grid_set_walkable(grid_t grid, position_t position, bool walkable);
bool grid_is_walkable(grid_t grid, position_t position);
