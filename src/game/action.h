#pragma once

#include "../lib/linkage.h"

#include <stdbool.h>

#include "entity.h"
#include "grid.h"
#include "pathing.h"
#include "turn.h"

// True on success (mover's mp -= BFS distance, position updated). False, no
// mutation, if: tile not walkable, tile occupied, or unreachable within
// mover's current mp. Distance comes from the caller-supplied
// `walking_distances`, not computed here -- the caller is responsible for
// supplying a dist grid BFS'd from `entity`'s current position (this
// function neither computes nor verifies that itself).
PUBLIC bool action_try_move(pathing_state_t walking_distances, grid_t grid, entity_t* entity, position_t target);

// True on success: attacker ap -= skill.ap_cost, defender takes skill.damage
// via entity_damage. Both entities must be alive (debug-asserted).
// False, no mutation, if:
// - same team
// - attacker.ap < skill.ap_cost
// - defender's position isn't among `attack_range_tiles`
//
// This function never computes the range check itself -- the caller
// supplies `attack_range_tiles` (e.g. game->pathing.attack_range_tiles,
// built once per attack-mode-entry via skill_target_in_range/
// pathing_in_range over the whole grid -- see game.c), and is responsible
// for it actually matching `skill`/`attacker`'s range; this function trusts
// it as given, the same way action_try_attack_area trusts its blast_tiles.
PUBLIC bool action_try_attack(entity_t* attacker, skill_t skill, entity_t* defender, slice_position_t attack_range_tiles);

// AoE counterpart of action_try_attack: targets a tile (the blast center)
// instead of a specific defender. True on success: attacker ap -=
// skill.ap_cost, every alive entity in `blast_tiles` whose team differs
// from attacker's takes skill.damage via entity_damage, and *out_hit is
// populated with exactly those damaged entities (staged on allocator;
// caller pops it when done). No friendly fire: same-team entities
// (including the attacker itself) are never damaged. False, no mutation,
// if: attacker.ap < skill.ap_cost, or impact isn't among
// `attack_range_tiles`. Only valid for AoE skills (skill.aoe_radius > 0,
// debug-asserted) -- single-target skills keep using action_try_attack.
//
// This function never computes the blast footprint or the range check
// itself -- the caller supplies `blast_tiles` (e.g. via
// pathing_compute_blast_tiles, or reused from a cached preview -- see
// game_cast_attack_area) and `attack_range_tiles` (e.g.
// game->pathing.attack_range_tiles, the same cache action_try_attack
// trusts -- built once per attack-mode-entry via pathing_in_range, which
// already applies skill.range and line of sight), and is responsible for
// `blast_tiles` actually matching `impact`; this function trusts both as
// given.
//
// Caller must have `allocator`'s cursor aligned to _Alignof(entity_ptr_t)
// before calling, with `blast_tiles` already staged below that cursor (not
// interleaved with it) -- this function does not self-align, matching this
// codebase's push-align-then-push convention (see entity_list_align et
// al.). *out_hit is staged starting at that aligned cursor, so a single
// `linear_allocator_pop(allocator, out_hit->slice)` followed by popping the
// caller's own alignment marker fully unwinds everything this call staged
// (blast_tiles, if the caller pushed it on `allocator` too, unwinds
// separately with its own pop).
PUBLIC bool action_try_attack_area(linear_allocator_t *allocator, slice_entity_t entities, entity_t *attacker, skill_t skill, position_t impact, slice_position_t attack_range_tiles, slice_position_t blast_tiles, slice_entity_ptr_t *out_hit);

#ifdef APP_UNITY_BUILD
#include "action.c"
#endif
