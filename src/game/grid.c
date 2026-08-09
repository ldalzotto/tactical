#include "grid.h"

#include "../lib/assert.h"

grid_t grid_init(linear_allocator_t *allocator, int width, int height) {
    assert_debug(width > 0 && height > 0);

    slice_tile_t tiles;
    tiles = LINEAR_ALLOCATOR_PUSH(allocator, tiles, (size_t)(width * height));

    for (int i = 0; i < width * height; i++) {
        SLICE_AT(tiles, i) = (tile_t){ .walkable = true };
    }

    grid_t grid = { .width = width, .height = height, .tiles = tiles };
    return grid;
}

void grid_deinit(linear_allocator_t *allocator, grid_t grid) {
    LINEAR_ALLOCATOR_POP(allocator, grid.tiles);
}

bool grid_in_bounds(grid_t grid, int x, int y) {
    return x >= 0 && y >= 0 && x < grid.width && y < grid.height;
}

tile_t *grid_tile_at(grid_t grid, int x, int y) {
    return &SLICE_AT(grid.tiles, y * grid.width + x);
}

void grid_set_walkable(grid_t grid, int x, int y, bool walkable) {
    grid_tile_at(grid, x, y)->walkable = walkable;
}

bool grid_is_walkable(grid_t grid, int x, int y) {
    return grid_tile_at(grid, x, y)->walkable;
}
