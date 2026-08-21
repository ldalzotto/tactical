#pragma once

#include "../lib/linkage.h"

#include <stdbool.h>

#include "entity.h"
#include "grid.h"

// True on success (mover's mp -= BFS distance, position updated). False, no
// mutation, if: tile not walkable, tile occupied, or unreachable within
// mover's current mp (via pathing_compute_walking_distances rooted at the mover).
PUBLIC bool action_try_move(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t* entity, position_t target);

// True on success: attacker ap -= skill.ap_cost, defender takes skill.damage
// via entity_damage. Both entities must be alive (debug-asserted).
// False, no mutation, if:
// - same team
// - attacker.ap < skill.ap_cost
// - defender out of skill.range (via skill_target_in_range)
PUBLIC bool action_try_attack(grid_t grid, slice_entity_t entities, entity_t* attacker, skill_t skill, entity_t* defender);

#ifdef APP_UNITY_BUILD
#include "action.c"
#endif
