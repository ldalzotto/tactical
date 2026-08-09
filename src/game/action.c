#include "action.h"

bool action_try_move(grid_t grid, entity_list_t entities, pathing_state_t pathing, entity_t* entity, int tx, int ty) {
    pathing_compute_distances(pathing, grid, entities, entity, entity->x, entity->y, entity->mp);

    int distance = pathing_distance_at(pathing, grid, tx, ty);
    if (distance < 0 || distance > entity->mp) {
        return false;
    }

    entity->mp -= distance;
    entity->x = tx;
    entity->y = ty;

    return true;
}

bool action_try_attack(entity_t* attacker, entity_t* defender) {

    if (!attacker->alive || !defender->alive) {
        return false;
    }

    if (attacker->team == defender->team) {
        return false;
    }

    if (!entity_is_adjacent(*attacker, *defender)) {
        return false;
    }

    if (attacker->ap == 0) {
        return false;
    }

    attacker->ap -= 1;
    entity_damage(defender, ACTION_ATTACK_DAMAGE);

    return true;
}
