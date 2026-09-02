#pragma once

#include "../lib/linkage.h"

#include <stdbool.h>

#include "entity.h"
#include "grid.h"
#include "pathing.h"
#include "turn.h"

// True on success (mp -= BFS distance, position updated); false with no
// mutation if the tile is unwalkable, occupied, or unreachable within mp.
// `walking_distances` is a BFS rooted at `entity`'s position (see
// pathing_compute_walking_distances).
PUBLIC bool action_try_move(pathing_state_t walking_distances, grid_t grid, entity_t* entity, position_t target);

// True on success: attacker.ap -= skill.ap_cost, defender takes skill.damage
// (entity_damage). Both must be alive (debug-asserted). False, no mutation,
// if same team, ap too low, or defender not in `attack_range_tiles`
// (caller-supplied, e.g. game->pathing.attack_range_tiles; see skill_can_target).
PUBLIC bool action_try_attack(entity_t* attacker, skill_t skill, entity_t* defender, slice_position_t attack_range_tiles);

// AoE counterpart of action_try_attack: targets a tile (blast center), not a
// defender. True on success: attacker.ap -= skill.ap_cost, every alive
// non-ally entity in `blast_tiles` takes skill.damage, and *out_hit lists
// exactly those entities (staged on allocator; caller pops it when done).
// No friendly fire. False, no mutation, if ap too low or `impact` isn't in
// `attack_range_tiles`. AoE skills only (skill.aoe_radius > 0, debug-asserted).
//
// `blast_tiles` (pathing_compute_blast_tiles, staged for `impact`) and
// `attack_range_tiles` are trusted as-is, not recomputed here.
//
// Caller must pre-align `allocator`'s cursor to _Alignof(entity_ptr_t) (see
// entity_list_align et al.); this function does not self-align. *out_hit is
// staged at that cursor, so popping out_hit->slice then the caller's
// alignment marker unwinds everything this call staged.
PUBLIC bool action_try_attack_area(linear_allocator_t *allocator, slice_entity_t entities, entity_t *attacker, skill_t skill, position_t impact, slice_position_t attack_range_tiles, slice_position_t blast_tiles, slice_entity_ptr_t *out_hit);

#ifdef APP_UNITY_BUILD
#include "action.c"
#endif
