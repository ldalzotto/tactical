#include "skill.h"

#include "pathing.h"

#include "../lib/assert.h"

const skill_t SKILL_MELEE = { .range = 1, .damage = 5, .ap_cost = 1 };
const skill_t SKILL_RANGED = { .range = 3, .damage = 3, .ap_cost = 1 };

PUBLIC bool skill_target_in_range(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t* attacker, skill_t skill, entity_t* target) {
    pathing_state_t pathing = pathing_compute_line_of_sight(allocator, grid, entities, attacker->position, skill.range);

    int distance = pathing_distance_at(pathing, grid, target->position);

    pathing_deinit(allocator, pathing);

    // pathing_compute_line_of_sight caps at skill.range, so a reachable
    // target can never be farther than the skill's range.
    assert_debug(distance <= skill.range);
    return distance >= 0;
}
