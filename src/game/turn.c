#include "turn.h"

turn_state_t turn_init(void) {
    return (turn_state_t){ .phase = TURN_PHASE_PLAYER, .turn_number = 1 };
}

void turn_reset_team_points(entity_list_t entities, entity_team_t team) {
    for ( SLICE_FOREACH(entities.entities, entity_s)) {
        entity_t *entity = &SLICE_DEREF(entity_s);
        if (entity->alive && entity->team == team) {
            entity->ap = entity->max_ap;
            entity->mp = entity->max_mp;
        }
    }
}

turn_state_t turn_begin_player_phase(turn_state_t state, entity_list_t entities) {
    state.phase = TURN_PHASE_PLAYER;
    state.turn_number++;
    turn_reset_team_points(entities, ENTITY_TEAM_PLAYER);
    return state;
}

turn_state_t turn_begin_enemy_phase(turn_state_t state, entity_list_t entities) {
    state.phase = TURN_PHASE_ENEMY;
    turn_reset_team_points(entities, ENTITY_TEAM_ENEMY);
    return state;
}
