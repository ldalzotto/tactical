#pragma once

#include "../lib/linkage.h"

#include "entity.h"
#include "grid.h"
#include "turn.h"

// Runs one enemy's turn (move toward the nearest reachable player, attack
// with the best available skill -- see ai.c's ai_preferred_skill /
// ai_best_in_range_skill). Returns every entity killed by this turn's
// attack: empty if no attack landed, exactly one for a single-target skill,
// possibly more for an AoE skill's blast.
//
// Caller must have `allocator`'s cursor aligned to _Alignof(entity_ptr_t)
// before calling (this function does not self-align -- same convention as
// action_try_attack_area). The returned slice is staged at that cursor;
// popping its slice then the caller's alignment marker unwinds everything
// this call staged.
PUBLIC slice_entity_ptr_t ai_run_ennemy_turn(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t *enemy);

#ifdef APP_UNITY_BUILD
#include "ai.c"
#endif
