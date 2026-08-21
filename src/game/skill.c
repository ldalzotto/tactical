#include "skill.h"

#include "pathing.h"

const skill_t SKILL_MELEE = { .range = 1, .damage = 5, .ap_cost = 1 };
const skill_t SKILL_RANGED = { .range = 3, .damage = 3, .ap_cost = 1 };
const skill_t SKILL_FIREBALL = { .range = 4, .aoe_radius = 2, .damage = 4, .ap_cost = 1 };

PUBLIC bool skill_target_in_range(grid_t grid, slice_entity_t entities, entity_t* attacker, skill_t skill, entity_t* target) {
    return pathing_in_range(grid, entities, attacker->position, target->position, skill.range);
}
