#pragma once

#include "../lib/linkage.h"

#include <stdbool.h>

#include "../lib/memory.h"
#include "entity.h"
#include "grid.h"

extern const skill_t SKILL_MELEE;
extern const skill_t SKILL_RANGED;

// True if `target` is reachable from `attacker`'s position within
// attacker->skill.range steps: BFS over walkable tiles, blocked by any other
// alive entity (allies or enemies) standing in the way. Attacker's own tile
// is never occupancy-checked (it's the BFS root); target's own tile is
// excluded from the occupancy check so it can be reached.
PUBLIC bool skill_target_in_range(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t* attacker, entity_t* target);

#ifdef APP_UNITY_BUILD
#include "skill.c"
#endif
