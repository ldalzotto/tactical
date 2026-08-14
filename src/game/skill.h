#pragma once

#include "../lib/linkage.h"

#include <stdbool.h>

#include "../lib/memory.h"
#include "entity.h"
#include "grid.h"

extern const skill_t SKILL_MELEE;
extern const skill_t SKILL_RANGED;

// True if `target` is within entity_active_skill(attacker).range steps of `attacker`:
// - BFS over walkable tiles, blocked by any other alive entity in the way
// - attacker's own tile is never occupancy-checked (it's the BFS root)
// - target's own tile is excluded from the occupancy check so it's reachable
PUBLIC bool skill_target_in_range(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t* attacker, entity_t* target);

#ifdef APP_UNITY_BUILD
#include "skill.c"
#endif
