#include "pathing.h"

#include "../lib/assert.h"

void pathing_deinit(linear_allocator_t *allocator, pathing_state_t state) {
    LINEAR_ALLOCATOR_POP(allocator, state.queue);
    LINEAR_ALLOCATOR_POP(allocator, state.dist);
    linear_allocator_pop(allocator, state.align);
}

pathing_state_t pathing_compute_distances(linear_allocator_t *allocator, grid_t grid, entity_list_t entities, entity_t* entity, int from_x, int from_y, int max_steps) {
    assert_debug(grid_in_bounds(grid, from_x, from_y));

    size_t count = (size_t)(grid.width * grid.height);

    slice_t align = linear_allocator_push_alignment(allocator, _Alignof(int32_t));

    slice_int32_t dist;
    dist = LINEAR_ALLOCATOR_PUSH(allocator, dist, count);

    slice_int32_t queue;
    queue = LINEAR_ALLOCATOR_PUSH(allocator, queue, count);

    pathing_state_t state = { .align = align, .dist = dist, .queue = queue };
    for (size_t i = 0; i < count; i++) {
        SLICE_AT(state.dist, i) = -1;
    }

    int head = 0;
    int tail = 0;

    int start_index = from_y * grid.width + from_x;
    SLICE_AT(state.dist, start_index) = 0;
    SLICE_AT(state.queue, tail) = start_index;
    tail++;

    static const int dx[4] = { 1, -1, 0, 0 };
    static const int dy[4] = { 0, 0, 1, -1 };

    while (head < tail) {
        int index = SLICE_AT(state.queue, head);
        head++;

        int x = index % grid.width;
        int y = index / grid.width;
        int d = SLICE_AT(state.dist, index);

        if (d >= max_steps) {
            continue;
        }

        for (int dir = 0; dir < 4; dir++) {
            int nx = x + dx[dir];
            int ny = y + dy[dir];

            if (!grid_in_bounds(grid, nx, ny)) {
                continue;
            }

            if (!grid_is_walkable(grid, nx, ny)) {
                continue;
            }

            int neighbor_index = ny * grid.width + nx;
            if (SLICE_AT(state.dist, neighbor_index) != -1) {
                continue;
            }

            entity_t* occupant = entity_find_at(entities, nx, ny);
            if (occupant != 0 && occupant != entity) {
                continue;
            }

            SLICE_AT(state.dist, neighbor_index) = d + 1;
            SLICE_AT(state.queue, tail) = neighbor_index;
            tail++;
        }
    }

    return state;
}

int pathing_distance_at(pathing_state_t state, grid_t grid, int x, int y) {
    if (!grid_in_bounds(grid, x, y)) {
        return -1;
    }

    return SLICE_AT(state.dist, y * grid.width + x);
}
