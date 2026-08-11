#pragma once

#include "entity.h"
#include "grid.h"

void ai_run_ennemy_turn(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t *enemy);
