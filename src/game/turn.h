#pragma once

#include "entity.h"

typedef enum {
    TURN_PHASE_PLAYER = 0,
    TURN_PHASE_ENEMY = 1,
} turn_phase_t;

typedef struct {
    turn_phase_t phase;
} turn_state_t;

turn_state_t turn_init(void);
void turn_reset_team_points(slice_entity_t entities, entity_team_t team);
turn_state_t turn_begin_player_phase(turn_state_t state, slice_entity_t entities);
turn_state_t turn_begin_enemy_phase(turn_state_t state, slice_entity_t entities);
