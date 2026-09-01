#include "grid.h"

#include "../lib/assert.h"
#include "game/position.h"
#include "lib/linkage.h"
#include "lib/memory.h"
#include <stddef.h>

struct tile {
    bool walkable;
    bool blocks_sight;
};

PUBLIC slice_t grid_align(linear_allocator_t *allocator) {
    return linear_allocator_push_alignment(allocator, _Alignof(tile_t));
}

PUBLIC grid_t grid_init(linear_allocator_t *allocator, int width, int height) {
    assert_debug(width > 0);
    assert_debug(height > 0);

    slice_tile_t tiles;
    tiles = LINEAR_ALLOCATOR_PUSH(allocator, tiles, (size_t)(width * height));

    for (SLICE_FOREACH(tiles, tile)) {
        SLICE_DEREF(tile) = (tile_t){ .walkable = true, .blocks_sight = false };
    }

    grid_t grid = { .width = width, .height = height, .tiles = tiles };
    return grid;
}

PUBLIC void grid_deinit(linear_allocator_t *allocator, grid_t grid) {
    LINEAR_ALLOCATOR_POP(allocator, grid.tiles);
}

PUBLIC bool grid_in_bounds(grid_t grid, position_t position) {
    return position.x >= 0 && position.y >= 0 && position.x < grid.width && position.y < grid.height;
}

PRIVATE tile_t *grid_tile_at(grid_t grid, position_t position) {
    return &SLICE_AT(grid.tiles, position.y * grid.width + position.x);
}

PUBLIC void grid_set_walkable(grid_t grid, position_t position, bool walkable) {
    grid_tile_at(grid, position)->walkable = walkable;
}

PUBLIC bool grid_is_walkable(grid_t grid, position_t position) {
    return grid_tile_at(grid, position)->walkable;
}

PUBLIC void grid_set_blocks_sight(grid_t grid, position_t position, bool blocks_sight) {
    grid_tile_at(grid, position)->blocks_sight = blocks_sight;
}

PUBLIC bool grid_blocks_sight(grid_t grid, position_t position) {
    return grid_tile_at(grid, position)->blocks_sight;
}

PUBLIC void grid_set_tile(grid_t grid, position_t position, tile_kind_t kind) {
    bool blocks_sight = (kind == TILE_WALL || kind == TILE_GRASS);
    bool walkable = (kind == TILE_GRASS || kind == TILE_FLOOR);
    grid_set_walkable(grid, position, walkable);
    grid_set_blocks_sight(grid, position, blocks_sight);
}

PUBLIC tile_kind_t grid_tile_kind(grid_t grid, position_t position) {
    bool walkable = grid_is_walkable(grid, position);
    bool blocks_sight = grid_blocks_sight(grid, position);

    if (blocks_sight) {
        return walkable ? TILE_GRASS : TILE_WALL;
    }
    return walkable ? TILE_FLOOR : TILE_CHASM;
}
