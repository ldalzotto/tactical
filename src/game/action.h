#pragma once

#include "../lib/linkage.h"

#include <stdbool.h>

#include "entity.h"
#include "grid.h"
#include "pathing.h"
#include "turn.h"

// True on success (mover's mp -= BFS distance, position updated). False, no
// mutation, if: tile not walkable, tile occupied, or unreachable within
// mover's current mp. Doesn't compute the BFS itself -- caller supplies
// `walking_distances`, rooted at `entity`'s current position (see
// pathing_compute_walking_distances, e.g. game->pathing.walking_distances
// or ai_step_toward's fresh per-call BFS).
PUBLIC bool action_try_move(pathing_state_t walking_distances, grid_t grid, entity_t* entity, position_t target);

// True on success: attacker ap -= skill.ap_cost, defender takes skill.damage
// via entity_damage. Both entities must be alive (debug-asserted).
// False, no mutation, if: same team, attacker.ap < skill.ap_cost, or
// defender's position isn't in `attack_range_tiles`. Doesn't compute the
// range check itself -- caller supplies `attack_range_tiles` (e.g.
// game->pathing.attack_range_tiles, built via skill_can_target/
// pathing_in_range) and is trusted to match `skill`/`attacker`'s range.
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
// Unlike action_try_attack, `impact` being in `attack_range_tiles` is a
// debug-asserted precondition rather than a runtime check: the only caller
// (game_cast_attack_area) already validates it via skill_can_target_area
// before calling, the same check `attack_range_tiles` itself is built from
// (see game_set_mode), so a caller reaching here with an out-of-range impact
// is a dispatch bug, not a normal rejected click.
//
// Doesn't compute the blast footprint or range check itself -- caller
// supplies `blast_tiles` (pathing_compute_blast_tiles, or -- the only actual
// caller -- game->pathing.blast_tiles, already staged for `impact`; see
// game_cast_attack_area) and `attack_range_tiles` (same trusted cache as
// action_try_attack), and is trusted that `blast_tiles` matches `impact`.
//
// Caller must have `allocator`'s cursor aligned to _Alignof(entity_ptr_t)
// before calling, with `blast_tiles` already staged below that cursor --
// this function does not self-align (see entity_list_align et al. for the
// push-align-then-push convention). *out_hit is staged at that aligned
// cursor; popping out_hit->slice then the caller's alignment marker
// unwinds everything this call staged (blast_tiles unwinds separately).
PUBLIC bool action_try_attack_area(linear_allocator_t *allocator, slice_entity_t entities, entity_t *attacker, skill_t skill, position_t impact, slice_position_t attack_range_tiles, slice_position_t blast_tiles, slice_entity_ptr_t *out_hit);

#ifdef APP_UNITY_BUILD
#include "action.c"
#endif
