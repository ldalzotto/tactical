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
// over walkable, unoccupied tiles (any OTHER alive entity blocks passage;
// skip_entity is excluded from the occupancy check -- pass the mover's own
// id, or ENTITY_ID_NONE when rooting at a bare target tile for AI).
// Distances beyond max_steps are left -1. Standard array-queue BFS,
// 4-directional neighbors, capacity width*height (can't overflow). Caller
// must pathing_deinit the result when done with it.
//
// pass_through_opposing_team_of: when non-null, any entity whose team
// differs from THIS entity's team is ALSO treated as passable (not just
// excluded) -- used for attack-range computation so targetable enemies
// don't block the BFS (and tiles behind them stay reachable-for-targeting).
// Allies (same team as pass_through_opposing_team_of) still block. Strictly
// this is "any other team," not "the opposing team" -- equivalent today
// since entity_team_t has exactly two values (PLAYER/ENEMY), but would need
// revisiting if a third (e.g. neutral) team were ever added. This is
// deliberately a separate parameter from `excluded`: excluded is whichever
// single entity's own tile must get a distance (the mover, or -- in
// skill_target_in_range -- the specific target being validated), which is
// not always the attacker whose team the pass-through rule should be
// relative to. Every pre-existing caller passes NULL (unchanged behavior).
PUBLIC pathing_state_t pathing_compute_distances(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t* excluded, position_t from, int max_steps, entity_t* pass_through_opposing_team_of);

PUBLIC int pathing_distance_at(pathing_state_t state, grid_t grid, position_t position); // -1 if unreached OR out of bounds -- this is a defensive query (arbitrary coords from clicks later), do NOT make it panic like grid_tile_at does

#ifdef APP_UNITY_BUILD
#include "pathing.c"
#endif
