#include "pathing.h"

#include "../lib/assert.h"
#include "geometry.h"

PUBLIC void pathing_deinit(linear_allocator_t *allocator, pathing_state_t state) {
    LINEAR_ALLOCATOR_POP(allocator, state.dist);
    linear_allocator_pop(allocator, state.align);
}

// Shared flood fill for pathing_compute_distances and, as a range/grid-bounds
// broad phase, pathing_compute_line_of_sight -- the latter passes an empty
// entities slice, so occupancy never blocks and dist ends up as plain
// Manhattan distance clipped to the grid and to walls; it then filters that
// down further with its own ray-based occlusion check.
PRIVATE pathing_state_t pathing_bfs(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, position_t from, int max_steps) {
    assert_debug(grid_in_bounds(grid, from));

    size_t count = (size_t)(grid.width * grid.height);

    slice_t align = linear_allocator_push_alignment(allocator, _Alignof(int32_t));

    slice_int32_t dist;
    dist = LINEAR_ALLOCATOR_PUSH(allocator, dist, count);

    slice_int32_t frontier;
    frontier = LINEAR_ALLOCATOR_PUSH(allocator, frontier, count);

    for (size_t i = 0; i < count; i++) {
        SLICE_AT(dist, i) = -1;
    }

    int head = 0;
    int tail = 0;

    int start_index = from.y * grid.width + from.x;
    SLICE_AT(dist, start_index) = 0;
    SLICE_AT(frontier, tail) = start_index;
    tail++;

    while (head < tail) {
        int index = SLICE_AT(frontier, head);
        head++;

        position_t current = { index % grid.width, index / grid.width };
        int d = SLICE_AT(dist, index);

        if (d >= max_steps) {
            continue;
        }

        for ( SLICE_FOREACH(POSITION_DIRECTIONS, dir_s)) {
            position_t dir = SLICE_DEREF(dir_s);
            position_t neighbor = position_add(current, dir);

            if (!grid_in_bounds(grid, neighbor)) {
                continue;
            }

            if (!grid_is_walkable(grid, neighbor)) {
                continue;
            }

            int neighbor_index = neighbor.y * grid.width + neighbor.x;
            // If already visited
            if (SLICE_AT(dist, neighbor_index) != -1) {
                continue;
            }

            if (entity_find_at(entities, neighbor) != 0) {
                continue;
            }

            SLICE_AT(dist, neighbor_index) = d + 1;
            SLICE_AT(frontier, tail) = neighbor_index;
            tail++;
        }
    }

    linear_allocator_pop(allocator, frontier.slice);

    return (pathing_state_t) {
        .align = align,
        .dist = dist,
    };
}

PUBLIC pathing_state_t pathing_compute_distances(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, position_t from, int max_steps) {
    return pathing_bfs(allocator, grid, entities, from, max_steps);
}

// True if the straight ray from `from` to `to` (`to` != `from`) is
// unobstructed: every intermediate tile -- both endpoints excluded, so
// neither `from`'s nor `to`'s own tile can block sight to itself -- does
// not block sight and is unoccupied.
PRIVATE bool pathing_line_of_sight_clear(grid_t grid, slice_entity_t entities, position_t from, position_t to) {
    geometry_line_iter_t it = geometry_line_iter_start(from, to);

    position_t tile;
    while (geometry_line_iter_next(&it, to, &tile)) {
        if (grid_blocks_sight(grid, tile)) {
            return false;
        }

        if (entity_find_at(entities, tile) != 0) {
            return false;
        }
    }

    return true;
}

PUBLIC pathing_state_t pathing_compute_line_of_sight(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, position_t from, int max_range) {
    // Broad phase: an entity-free flood fill gives every walkable tile
    // within max_range Manhattan steps, clipped to the grid -- a diamond,
    // ignoring who's standing where. Narrow phase below strips out anything
    // not actually visible along a straight ray from `from`.
    pathing_state_t state = pathing_bfs(allocator, grid, (slice_entity_t){0}, from, max_range);

    for (int ty = 0; ty < grid.height; ty++) {
        for (int tx = 0; tx < grid.width; tx++) {
            int index = ty * grid.width + tx;
            if (SLICE_AT(state.dist, index) <= 0) {
                continue;
            }

            position_t tile = { tx, ty };
            if (!pathing_line_of_sight_clear(grid, entities, from, tile)) {
                SLICE_AT(state.dist, index) = -1;
            }
        }
    }

    return state;
}

PUBLIC int pathing_distance_at(pathing_state_t state, grid_t grid, position_t position) {
    assert_debug(grid_in_bounds(grid, position));
    return SLICE_AT(state.dist, position.y * grid.width + position.x);
}
