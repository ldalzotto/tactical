#pragma once

#include "entity.h"
#include "grid.h"
#include "pathing.h"

// For every ALIVE enemy (ascending entity_id): find nearest ALIVE player
// entity via BFS rooted at the enemy's own tile (skip_entity=enemy). If
// already orthogonally adjacent and ap>0, action_try_attack. Otherwise, while
// mp>0 and not yet adjacent: BFS rooted at the TARGET's tile
// (skip_entity=ENTITY_ID_NONE) to get a distance-to-target field, step to
// whichever orthogonal neighbor (checked in order: up, right, down, left) of
// the enemy's current tile has the smallest distance-to-target via
// action_try_move (one tile at a time), then re-check adjacency/attack. If no
// alive player entities remain, no-op for all enemies.
void ai_run_enemy_phase(grid_t grid, entity_list_t entities, pathing_state_t pathing);
