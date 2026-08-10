#pragma once

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

void pathing_deinit(linear_allocator_t *allocator, pathing_state_t state);

// Allocates a pathing_state_t from allocator (width*height dist + queue,
// grid.width*grid.height derived from grid), then BFS from `from`
// over walkable, unoccupied tiles (any OTHER alive entity blocks passage;
// skip_entity is excluded from the occupancy check -- pass the mover's own
// id, or ENTITY_ID_NONE when rooting at a bare target tile for AI).
// Distances beyond max_steps are left -1. Standard array-queue BFS,
// 4-directional neighbors, capacity width*height (can't overflow). Caller
// must pathing_deinit the result when done with it.
pathing_state_t pathing_compute_distances(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t* excluded, position_t from, int max_steps);

int pathing_distance_at(pathing_state_t state, grid_t grid, position_t position); // -1 if unreached OR out of bounds -- this is a defensive query (arbitrary coords from clicks later), do NOT make it panic like grid_tile_at does
