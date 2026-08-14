#include "grid.h"

#include "../lib/assert.h"

struct tile {
    bool walkable;
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
        SLICE_DEREF(tile) = (tile_t){ .walkable = true };
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
