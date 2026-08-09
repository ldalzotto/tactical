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
bool action_try_move(grid_t grid, entity_list_t entities, pathing_state_t pathing, entity_id_t mover, int tx, int ty);

// True on success (attacker ap -= 1, defender takes ACTION_ATTACK_DAMAGE via
// entity_damage). False, no mutation, if either is dead, same team, not
// orthogonally adjacent, or attacker.ap == 0.
bool action_try_attack(entity_list_t entities, entity_id_t attacker, entity_id_t defender);
