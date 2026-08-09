#pragma once

#include <stdbool.h>

#include "../lib/memory.h"
#include "../lib/runtime.h"
#include "entity.h"
#include "grid.h"
#include "layout.h"
#include "pathing.h"
#include "turn.h"

typedef enum {
    GAME_OVER_NONE = 0,
    GAME_OVER_WIN = 1,
    GAME_OVER_LOSE = 2,
} game_over_t;

typedef struct {
    grid_t grid;
    entity_list_t entities;
    // TODO: the pathing should not be a state. It should only expose a function that returns a dispoable result.
    pathing_state_t pathing;
    turn_state_t turn;
    viewport_t viewport;
    entity_t* selected_entity; // 0 if none
    int hover_x, hover_y;
    bool hover_valid;
    game_over_t game_over;
} game_state_t;

// Allocates grid/entities/pathing (push order: grid, then entity-alignment +
// entities, then int32-alignment + pathing). Does NOT spawn entities or set
// obstacles -- that is scenario content, not orchestration.
game_state_t game_init(linear_allocator_t *allocator, int grid_width, int grid_height, int fb_width, int fb_height, int hud_height);

// Pops grid/entities/pathing (plus the alignment padding pushed between
// them) in exact reverse of game_init's push order.
void game_deinit(linear_allocator_t *allocator, game_state_t state);

void game_on_entity_pressed(game_state_t *game, entity_t* entity);
void game_on_tile_pressed(game_state_t *game, int tx, int ty);
void game_on_end_turn_pressed(game_state_t *game);
void game_on_input_event(game_state_t *game, input_event_t event);
