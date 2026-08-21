#pragma once

#include "../lib/linkage.h"

#include <stdbool.h>

#include "../lib/memory.h"
#include "position.h"

typedef struct tile tile_t;

SLICE_DEFINE(tile_t);

typedef struct {
    int width, height;
    slice_tile_t tiles;
} grid_t;

// Named points in the walkable/blocks_sight plane (tile_t's two bools).
typedef enum {
    TILE_FLOOR,  // walkable, sight-clear
    TILE_WALL,   // not walkable, blocks sight
    TILE_GRASS,  // walkable, blocks sight
    TILE_CHASM,  // not walkable, sight-clear
} tile_kind_t;

PUBLIC slice_t grid_align(linear_allocator_t *allocator);
PUBLIC grid_t grid_init(linear_allocator_t *allocator, int width, int height);
PUBLIC void grid_deinit(linear_allocator_t *allocator, grid_t grid);
PUBLIC bool grid_in_bounds(grid_t grid, position_t position);
PUBLIC void grid_set_walkable(grid_t grid, position_t position, bool walkable);
PUBLIC bool grid_is_walkable(grid_t grid, position_t position);
PUBLIC void grid_set_blocks_sight(grid_t grid, position_t position, bool blocks_sight);
PUBLIC bool grid_blocks_sight(grid_t grid, position_t position);
PUBLIC void grid_set_tile(grid_t grid, position_t position, tile_kind_t kind);
PUBLIC tile_kind_t grid_tile_kind(grid_t grid, position_t position);

#ifdef APP_UNITY_BUILD
#include "grid.c"
#endif
