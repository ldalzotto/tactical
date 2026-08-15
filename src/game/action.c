#include "action.h"
#include "pathing.h"
#include "skill.h"

#include "../lib/assert.h"

PUBLIC bool action_try_move(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t* entity, position_t target) {
    pathing_state_t pathing = pathing_compute_distances(allocator, grid, entities, entity->position, entity->mp);

    int distance = pathing_distance_at(pathing, grid, target);

    pathing_deinit(allocator, pathing);

    if (distance < 0) {
        return false;
    }
    // pathing_compute_distances caps the BFS at entity->mp, so a reachable
    // tile can never have a distance greater than the mover's remaining mp.
    assert_debug(distance <= entity->mp);

    entity->mp -= distance;
    entity->position = target;

    return true;
}

PUBLIC bool action_try_attack(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t* attacker, skill_t skill, entity_t* defender) {
    assert_debug(attacker->alive);
    assert_debug(defender->alive);

    if (attacker->team == defender->team) {
        return false;
    }

    if (attacker->ap < skill.ap_cost) {
        return false;
    }

    if (!skill_target_in_range(allocator, grid, entities, attacker, skill, defender)) {
        return false;
    }

    attacker->ap -= skill.ap_cost;
    entity_damage(defender, skill.damage);

    return true;
}
