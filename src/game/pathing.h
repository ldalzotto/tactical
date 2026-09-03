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

// Allocates a pathing_state_t (width*height dist + queue), then BFS from
// `from` over walkable, unoccupied tiles (any alive entity blocks passage;
// the root is seeded before neighbors are explored, so the mover never
// blocks its own path). Distances beyond max_steps stay -1. Standard
// array-queue BFS, 4-directional, capacity width*height. Caller must
// pathing_deinit the result.
//
// This is walking distance -- it routes around obstacles, the right metric
// for movement range. Skill range is different (see pathing_can_target) and
// doesn't use this function.
PUBLIC pathing_state_t pathing_compute_walking_distances(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, position_t from, int max_steps);

// True if `to` is within Manhattan max_range of `from` AND has clear line of
// sight -- a straight ray unobstructed by a sight-blocking tile
// (grid_set_blocks_sight, independent of walkability) or an intermediate
// entity (`to` itself is exempt, so a unit doesn't occlude itself) -- AND
// `to` isn't empty sight-blocking ground (nothing there to target).
// Unlike pathing_compute_walking_distances, this never routes around
// obstacles: hidden-behind-cover targets are out of range even with a
// walkable detour, while non-walkable-but-sight-clear tiles (chasm, window)
// are in range as long as ray and distance both clear.
//
// Point query, O(max_range) -- no allocator, no grid-wide scan.
PUBLIC bool pathing_can_target(grid_t grid, slice_entity_t entities, position_t from, position_t to, int max_range);

PUBLIC int pathing_distance_at(pathing_state_t state, grid_t grid, position_t position); // -1 if unreached or out of bounds; defensive (click coords), don't panic like grid_tile_at

// The blast footprint of an AoE impact: every tile within Manhattan
// `radius` of `center`, ignoring obstacles (no LOS or occupancy check). No
// per-tile falloff data, just a plain tile list, staged on `allocator` like
// game.c's mode-switch scans. Caller pops it when done
// (linear_allocator_pop(allocator, result.slice) -- no _deinit, unlike
// pathing_state_t).
PUBLIC slice_position_t pathing_compute_blast_tiles(linear_allocator_t *allocator, grid_t grid, position_t center, int radius);

// Result of pathing_compute_attack_range: attack_range_tiles and
// los_blocked_tiles are one contiguous allocation (no gap), so callers
// copy both at once. `align` marks the bottom of everything staged
// (including internal partition scratch) -- pop the whole span via it,
// not the two tile slices individually.
typedef struct {
    slice_t align;
    slice_position_t attack_range_tiles;
    slice_position_t los_blocked_tiles;
} pathing_attack_range_t;

// In-range tiles from `from` (excluding `from` itself), split into
// attack_range_tiles (selectable) and los_blocked_tiles (in range but
// LOS-blocked, shown dimmed instead of hidden). Ally-occupied tiles are
// dropped entirely. Staged on `allocator` like pathing_compute_blast_tiles.
PUBLIC pathing_attack_range_t pathing_compute_attack_range(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, position_t from, int max_range);

#ifdef APP_UNITY_BUILD
#include "pathing.c"
#endif
