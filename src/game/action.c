#include "action.h"

bool action_try_move(grid_t grid, entity_list_t entities, pathing_state_t pathing, entity_id_t mover, int tx, int ty) {
    entity_t *entity = entity_at(entities, mover);

    pathing_compute_distances(pathing, grid, entities, mover, entity->x, entity->y, entity->mp);

    int distance = pathing_distance_at(pathing, grid, tx, ty);
    if (distance < 0 || distance > entity->mp) {
        return false;
    }

    entity->mp -= distance;
    entity->x = tx;
    entity->y = ty;

    return true;
}

bool action_try_attack(entity_list_t entities, entity_id_t attacker, entity_id_t defender) {
    entity_t *attacker_entity = entity_at(entities, attacker);
    entity_t *defender_entity = entity_at(entities, defender);

    if (!attacker_entity->alive || !defender_entity->alive) {
        return false;
    }

    if (attacker_entity->team == defender_entity->team) {
        return false;
    }

    if (!entity_is_adjacent(*attacker_entity, *defender_entity)) {
        return false;
    }

    if (attacker_entity->ap == 0) {
        return false;
    }

    attacker_entity->ap -= 1;
    entity_damage(entities, defender, ACTION_ATTACK_DAMAGE);

    return true;
}
