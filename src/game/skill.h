#pragma once

#include "../lib/linkage.h"

#include <stdbool.h>

#include "../lib/memory.h"
#include "entity.h"
#include "grid.h"

extern const skill_t SKILL_MELEE;
extern const skill_t SKILL_RANGED;

// True if `target` is within `skill`.range Manhattan steps of `attacker`
// AND there's a clear line of sight -- a straight ray from attacker to
// target unobstructed by non-walkable terrain or any other entity standing
// on an intermediate tile (see pathing_compute_line_of_sight).
PUBLIC bool skill_target_in_range(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t* attacker, skill_t skill, entity_t* target);

#ifdef APP_UNITY_BUILD
#include "skill.c"
#endif
