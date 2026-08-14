#include "action.h"
#include "pathing.h"
#include "skill.h"

PUBLIC bool action_try_move(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t* entity, position_t target) {
    pathing_state_t pathing = pathing_compute_distances(allocator, grid, entities, entity, entity->position, entity->mp, false);

    int distance = pathing_distance_at(pathing, grid, target);

    pathing_deinit(allocator, pathing);

    if (distance < 0 || distance > entity->mp) {
        return false;
    }

    entity->mp -= distance;
    entity->position = target;

    return true;
}

PUBLIC bool action_try_attack(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t* attacker, entity_t* defender) {

    if (!attacker->alive || !defender->alive) {
        return false;
    }

    if (attacker->team == defender->team) {
        return false;
    }

    skill_t skill = entity_active_skill(attacker);

    if (attacker->ap < skill.ap_cost) {
        return false;
    }

    if (!skill_target_in_range(allocator, grid, entities, attacker, defender)) {
        return false;
    }

    attacker->ap -= skill.ap_cost;
    entity_damage(defender, skill.damage);

    return true;
}
