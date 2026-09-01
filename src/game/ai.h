#pragma once

#include "../lib/linkage.h"

#include "entity.h"
#include "grid.h"
#include "turn.h"

// Runs one enemy's turn: move toward the nearest player, attack with the
// best skill in range. Returns every entity killed (0, 1, or more for AoE).
//
// Caller must pre-align `allocator`'s cursor to _Alignof(entity_ptr_t)
// (this function does not self-align, like action_try_attack_area). Unwind
// by popping the returned slice, then the caller's alignment marker.
PUBLIC slice_entity_ptr_t ai_run_ennemy_turn(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t *enemy);

#ifdef APP_UNITY_BUILD
#include "ai.c"
#endif
