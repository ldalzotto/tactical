#pragma once

#include <stdbool.h>

#include "entity.h"
#include "grid.h"
#include "pathing.h"

#define ACTION_ATTACK_DAMAGE 5

// True on success (mover's mp -= BFS distance, position updated). False, no
// mutation, if: tile not walkable, tile occupied, or unreachable within
// mover's current mp (via pathing_compute_distances rooted at the mover,
// skip_entity = mover).
bool action_try_move(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t* entity, position_t target);

// True on success (attacker ap -= 1, defender takes ACTION_ATTACK_DAMAGE via
// entity_damage). False, no mutation, if either is dead, same team, not
// orthogonally adjacent, or attacker.ap == 0.
bool action_try_attack(entity_t* attacker, entity_t* defender);
