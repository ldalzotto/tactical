#include "pathing.h"

#include "../lib/assert.h"

void pathing_deinit(linear_allocator_t *allocator, pathing_state_t state) {
    LINEAR_ALLOCATOR_POP(allocator, state.dist);
    linear_allocator_pop(allocator, state.align);
}

pathing_state_t  pathing_compute_distances(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t* excluded, position_t from, int max_steps) {
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

        for (int dir = 0; dir < 4; dir++) {
            position_t neighbor = position_add(current, POSITION_DIRECTIONS[dir]);

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

            entity_t* occupant = entity_find_at(entities, neighbor);
            if (occupant != 0 && occupant != excluded) {
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

int pathing_distance_at(pathing_state_t state, grid_t grid, position_t position) {
    assert_debug(grid_in_bounds(grid, position));
    return SLICE_AT(state.dist, position.y * grid.width + position.x);
}
