#pragma once

#include "../lib/linkage.h"

#include "entity.h"
#include "grid.h"

// Returns the entity that was attacked this turn, or 0 if no attack landed.
PUBLIC entity_t* ai_run_ennemy_turn(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t *enemy);

#ifdef APP_UNITY_BUILD
#include "ai.c"
#endif
