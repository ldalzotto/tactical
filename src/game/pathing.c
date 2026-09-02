#include "pathing.h"

#include "../lib/assert.h"
#include "game/entity.h"
#include "game/grid.h"
#include "game/position.h"
#include "geometry.h"
#include "lib/linkage.h"
#include "lib/memory.h"
#include <stddef.h>
#include <stdint.h>

PUBLIC void pathing_deinit(linear_allocator_t *allocator, pathing_state_t state) {
    LINEAR_ALLOCATOR_POP(allocator, state.dist);
    linear_allocator_pop(allocator, state.align);
}

// Flood fill for pathing_compute_walking_distances.
PRIVATE pathing_state_t pathing_bfs(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, position_t from, int max_steps) {
    assert_debug(grid_in_bounds(grid, from));

    size_t count = (size_t)(grid.width * grid.height);

    slice_t align = linear_allocator_push_alignment(allocator, _Alignof(int32_t));

    slice_int32_t dist;
    dist = LINEAR_ALLOCATOR_PUSH(allocator, dist, count);

    slice_int32_t frontier;
    frontier = LINEAR_ALLOCATOR_PUSH(allocator, frontier, count);

    // Occupancy bitmap: O(N) to build, but turns each neighbor check into
    // O(1) instead of an O(N) entity_find_at scan.
    slice_uint8_t occupied;
    occupied = LINEAR_ALLOCATOR_PUSH(allocator, occupied, count);

    for (size_t i = 0; i < count; i++) {
        SLICE_AT(dist, i) = -1;
        SLICE_AT(occupied, i) = 0;
    }

    for (SLICE_FOREACH(entities, entity_s)) {
        entity_t *entity = &SLICE_DEREF(entity_s);
        if (!entity->alive) {
            continue;
        }
        int index = entity->position.y * grid.width + entity->position.x;
        SLICE_AT(occupied, index) = 1;
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
            if (SLICE_AT(dist, neighbor_index) != -1) {
                continue;
            }

            if (SLICE_AT(occupied, neighbor_index) != 0) {
                continue;
            }

            SLICE_AT(dist, neighbor_index) = d + 1;
            SLICE_AT(frontier, tail) = neighbor_index;
            tail++;
        }
    }

    linear_allocator_pop(allocator, occupied.slice);
    linear_allocator_pop(allocator, frontier.slice);

    return (pathing_state_t) {
        .align = align,
        .dist = dist,
    };
}

PUBLIC pathing_state_t pathing_compute_walking_distances(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, position_t from, int max_steps) {
    return pathing_bfs(allocator, grid, entities, from, max_steps);
}

PRIVATE int pathing_manhattan_distance(position_t a, position_t b) {
    int dx = a.x - b.x;
    int dy = a.y - b.y;
    return (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
}

// True if the straight ray from `from` to `to` (`to` != `from`) is
// unobstructed: every intermediate tile (endpoints excluded, so neither can
// block sight to itself) is sight-clear and unoccupied.
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

PUBLIC bool pathing_can_target(grid_t grid, slice_entity_t entities, position_t from, position_t to, int max_range) {
    if (pathing_manhattan_distance(from, to) > max_range) {
        return false;
    }

    if (!pathing_line_of_sight_clear(grid, entities, from, to)) {
        return false;
    }

    // pathing_line_of_sight_clear skips `to` (so a standing entity doesn't
    // occlude itself), but empty sight-blocking ground has nothing to target.
    if (grid_blocks_sight(grid, to) && entity_find_at(entities, to) == 0) {
        return false;
    }

    return true;
}

PUBLIC int pathing_distance_at(pathing_state_t state, grid_t grid, position_t position) {
    assert_debug(grid_in_bounds(grid, position));
    return SLICE_AT(state.dist, position.y * grid.width + position.x);
}

PUBLIC slice_position_t pathing_compute_blast_tiles(linear_allocator_t *allocator, grid_t grid, position_t center, int radius) {
    slice_position_t tiles;
    tiles = LINEAR_ALLOCATOR_PUSH(allocator, tiles, 0);

    for (int ty = 0; ty < grid.height; ty++) {
        for (int tx = 0; tx < grid.width; tx++) {
            position_t position = { tx, ty };

            if (pathing_manhattan_distance(center, position) > radius) {
                continue;
            }

            slice_position_t entry = LINEAR_ALLOCATOR_PUSH(allocator, tiles, 1);
            SLICE_DEREF(entry) = position;
            tiles.end = entry.end;
        }
    }

    return tiles;
}

PUBLIC slice_position_t pathing_compute_attack_range(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, position_t from, int max_range) {
    slice_position_t tiles;
    tiles = LINEAR_ALLOCATOR_PUSH(allocator, tiles, 0);

    // A tile occupied by the attacker's own team is never a valid target or
    // AoE impact, even if in range/LOS. `from` is the attacker's own
    // position, so it's always occupied.
    entity_t *self = entity_find_at(entities, from);
    assert_debug(self != 0);

    for (int ty = 0; ty < grid.height; ty++) {
        for (int tx = 0; tx < grid.width; tx++) {
            position_t position = { tx, ty };
            // from == to is trivially in range but never a valid target.
            if (position_equals(position, from)) {
                continue;
            }

            if (!pathing_can_target(grid, entities, from, position, max_range)) {
                continue;
            }

            entity_t *occupant = entity_find_at(entities, position);
            if (occupant != 0 && occupant->team == self->team) {
                continue;
            }

            slice_position_t entry = LINEAR_ALLOCATOR_PUSH(allocator, tiles, 1);
            SLICE_DEREF(entry) = position;
            tiles.end = entry.end;
        }
    }

    return tiles;
}
