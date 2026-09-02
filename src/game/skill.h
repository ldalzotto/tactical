#pragma once

#include "../lib/linkage.h"

#include <stdbool.h>

#include "../lib/memory.h"
#include "entity.h"
#include "grid.h"

extern const skill_t SKILL_MELEE;
extern const skill_t SKILL_RANGED;
extern const skill_t SKILL_FIREBALL;

// True if `target` is within `skill`.range Manhattan steps of `attacker`
// with clear line of sight (see pathing_can_target).
PUBLIC bool skill_can_target(grid_t grid, slice_entity_t entities, entity_t* attacker, skill_t skill, entity_t* target);

// True if `skill` has an area-of-effect footprint (as opposed to a
// single-target skill).
PUBLIC bool skill_is_aoe(skill_t skill);

#ifdef APP_UNITY_BUILD
#include "skill.c"
#endif
