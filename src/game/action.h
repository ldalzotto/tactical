#pragma once

#include "../lib/linkage.h"

#include <stdbool.h>

#include "entity.h"
#include "grid.h"
#include "pathing.h"
#include "turn.h"

// True on success (mover's mp -= BFS distance, position updated). False, no
// mutation, if: tile not walkable, tile occupied, or unreachable within
// mover's current mp. Caller supplies `walking_distances`, a BFS rooted at
// `entity`'s position (see pathing_compute_walking_distances).
PUBLIC bool action_try_move(pathing_state_t walking_distances, grid_t grid, entity_t* entity, position_t target);

// True on success: attacker ap -= skill.ap_cost, defender takes skill.damage
// via entity_damage. Both entities must be alive (debug-asserted).
// False, no mutation, if: same team, attacker.ap < skill.ap_cost, or
// defender's position isn't in `attack_range_tiles` (caller-supplied, e.g.
// game->pathing.attack_range_tiles; see skill_can_target).
PUBLIC bool action_try_attack(entity_t* attacker, skill_t skill, entity_t* defender, slice_position_t attack_range_tiles);

// AoE counterpart of action_try_attack: targets a tile (the blast center)
// instead of a specific defender. True on success: attacker ap -=
// skill.ap_cost, every alive entity in `blast_tiles` whose team differs
// from attacker's takes skill.damage via entity_damage, and *out_hit is
// populated with exactly those damaged entities (staged on allocator;
// caller pops it when done). No friendly fire. False, no mutation, if
// attacker.ap < skill.ap_cost. Only valid for AoE skills (skill.aoe_radius >
// 0, debug-asserted).
//
// `impact` being in `attack_range_tiles` is a debug-asserted precondition,
// not a runtime check: the only caller (game_try_cast_attack_area) already
// validates it against game->pathing.attack_range_tiles before calling.
//
// Caller supplies `blast_tiles` (pathing_compute_blast_tiles, already
// staged for `impact`) and `attack_range_tiles`; both are trusted, not
// recomputed here.
//
// Caller must have `allocator`'s cursor aligned to _Alignof(entity_ptr_t)
// before calling (this function does not self-align -- see entity_list_align
// et al. for the push-align-then-push convention). *out_hit is staged at
// that cursor; popping out_hit->slice then the caller's alignment marker
// unwinds everything this call staged.
PUBLIC bool action_try_attack_area(linear_allocator_t *allocator, slice_entity_t entities, entity_t *attacker, skill_t skill, position_t impact, slice_position_t attack_range_tiles, slice_position_t blast_tiles, slice_entity_ptr_t *out_hit);

#ifdef APP_UNITY_BUILD
#include "action.c"
#endif
