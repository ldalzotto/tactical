#pragma once

#include "../lib/linkage.h"

#include <stdbool.h>

#include "entity.h"
#include "grid.h"

// True on success (mover's mp -= BFS distance, position updated). False, no
// mutation, if: tile not walkable, tile occupied, or unreachable within
// mover's current mp (via pathing_compute_distances rooted at the mover,
// skip_entity = mover).
PUBLIC bool action_try_move(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t* entity, position_t target);

// True on success: attacker ap -= entity_active_skill(attacker).ap_cost, defender
// takes entity_active_skill(attacker).damage via entity_damage. False, no
// mutation, if:
// - either entity is dead, or same team
// - attacker.ap < entity_active_skill(attacker).ap_cost
// - defender out of entity_active_skill(attacker).range (via skill_target_in_range)
PUBLIC bool action_try_attack(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t* attacker, entity_t* defender);

#ifdef APP_UNITY_BUILD
#include "action.c"
#endif
