#pragma once

#include <stdint.h>

#include "../lib/memory.h"
#include "entity.h"
#include "grid.h"

SLICE_DEFINE(int32_t);

typedef struct {
    slice_int32_t dist;   // width*height, -1 = unreached
    slice_int32_t queue;  // width*height capacity, BFS frontier scratch
} pathing_state_t;

pathing_state_t pathing_init(linear_allocator_t *allocator, int grid_width, int grid_height);
void pathing_deinit(linear_allocator_t *allocator, pathing_state_t state);

// BFS from (from_x,from_y) over walkable, unoccupied tiles (any OTHER alive
// entity blocks passage; skip_entity is excluded from the occupancy check --
// pass the mover's own id, or ENTITY_ID_NONE when rooting at a bare target
// tile for AI). Distances beyond max_steps are left -1. Standard array-queue
// BFS, 4-directional neighbors, capacity width*height (can't overflow).
void pathing_compute_distances(pathing_state_t state, grid_t grid, entity_list_t entities, entity_id_t skip_entity, int from_x, int from_y, int max_steps);

int pathing_distance_at(pathing_state_t state, grid_t grid, int x, int y); // -1 if unreached OR out of bounds -- this is a defensive query (arbitrary coords from clicks later), do NOT make it panic like grid_tile_at does
