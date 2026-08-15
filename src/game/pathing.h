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
PUBLIC pathing_state_t pathing_compute_distances(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, position_t from, int max_steps);

// Range for a skill: tiles within Manhattan distance max_range of `from`
// that also have a clear line of sight -- a straight ray from `from` to the
// tile, unobstructed by non-walkable terrain or by any other entity standing
// on an intermediate tile (the two endpoints are never checked, so neither
// the mover's own tile nor the target's own tile can block its ray). Unlike
// pathing_compute_distances, this never routes around obstacles: a target
// hidden behind a wall or a standing unit is out of range even if a walkable
// detour exists.
//
// dist is Manhattan distance for tiles with clear LOS, -1 otherwise
// (including tiles beyond max_range, which are never ray-traced).
PUBLIC pathing_state_t pathing_compute_line_of_sight(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, position_t from, int max_range);

PUBLIC int pathing_distance_at(pathing_state_t state, grid_t grid, position_t position); // -1 if unreached OR out of bounds -- this is a defensive query (arbitrary coords from clicks later), do NOT make it panic like grid_tile_at does

#ifdef APP_UNITY_BUILD
#include "pathing.c"
#endif
