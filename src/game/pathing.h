#pragma once

#include "../lib/linkage.h"

#include <stdbool.h>
#include <stdint.h>

#include "../lib/memory.h"
#include "entity.h"
#include "grid.h"
#include "position.h"

SLICE_DEFINE(int32_t);

typedef struct {
    slice_t align;         // internal: alignment padding pushed before dist/queue
    slice_int32_t dist;    // width*height, -1 = unreached
} pathing_state_t;

PUBLIC void pathing_deinit(linear_allocator_t *allocator, pathing_state_t state);

// Allocates a pathing_state_t from allocator (width*height dist + queue,
// grid.width*grid.height derived from grid), then BFS from `from`
// over walkable, unoccupied tiles (any alive entity blocks passage --
// the root tile itself is seeded before neighbors are explored, so the
// mover standing there never blocks its own path).
// Distances beyond max_steps are left -1. Standard array-queue BFS,
// 4-directional neighbors, capacity width*height (can't overflow). Caller
// must pathing_deinit the result when done with it.
//
// This is walking distance: it routes around obstacles. It's the right
// metric for movement range. Skill range is a different metric -- see
// pathing_in_range -- and does not use this function.
PUBLIC pathing_state_t pathing_compute_walking_distances(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, position_t from, int max_steps);

// True if `to` is within Manhattan distance max_range of `from` AND there's
// a clear line of sight -- a straight ray from `from` to `to`, unobstructed
// by a sight-blocking tile (grid_set_blocks_sight, independent of
// walkability) or by any other entity standing on an intermediate tile (the
// two endpoints are never checked, so neither `from`'s nor `to`'s own tile
// can block the ray). Unlike pathing_compute_walking_distances, this never
// routes around obstacles: a target hidden behind a sight-blocking tile or
// a standing unit is out of range even if a walkable detour exists, and a
// target beyond a non-walkable-but-sight-clear tile (a chasm, a window) is
// in range as long as the ray and the Manhattan distance both clear.
//
// Point query, O(max_range) -- no allocator, no grid-wide scan.
PUBLIC bool pathing_in_range(grid_t grid, slice_entity_t entities, position_t from, position_t to, int max_range);

// True if the straight ray from `from` to `to` (`to` != `from`) is
// unobstructed: every intermediate tile -- both endpoints excluded, so
// neither `from`'s nor `to`'s own tile can block sight to itself -- does
// not block sight and is unoccupied. Ignores distance entirely; pair with
// a range check (see pathing_in_range) if that matters to the caller.
PUBLIC bool pathing_line_of_sight_clear(grid_t grid, slice_entity_t entities, position_t from, position_t to);

PUBLIC int pathing_distance_at(pathing_state_t state, grid_t grid, position_t position); // -1 if unreached OR out of bounds -- this is a defensive query (arbitrary coords from clicks later), do NOT make it panic like grid_tile_at does

#ifdef APP_UNITY_BUILD
#include "pathing.c"
#endif
