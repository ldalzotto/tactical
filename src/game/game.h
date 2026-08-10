#pragma once

#include <stdbool.h>

#include "../lib/memory.h"
#include "../lib/runtime.h"
#include "entity.h"
#include "grid.h"
#include "layout.h"
#include "position.h"
#include "turn.h"

typedef enum {
    GAME_OVER_NONE = 0,
    GAME_OVER_WIN = 1,
    GAME_OVER_LOSE = 2,
} game_over_t;

typedef struct {
    grid_t grid;
    slice_entity_t entities;
    turn_state_t turn;
    viewport_t viewport;
    entity_t* selected_entity; // 0 if none
    position_t hover;
    bool hover_valid;
    game_over_t game_over;
} game_state_t;

// Assembles game state from an already-allocated grid and entity list.
// The caller owns allocation (grid must be pushed before entities, so the
// whole region can be popped as one block in game_deinit).
game_state_t game_init(grid_t grid, slice_entity_t entities, int fb_width, int fb_height, int hud_height);

// Pops the grid+entities region in one shot (grid.tiles.begin..entities.end).
void game_deinit(linear_allocator_t *allocator, game_state_t state);

void game_on_entity_pressed(game_state_t *game, entity_t* entity);
void game_on_tile_pressed(game_state_t *game, linear_allocator_t *allocator, position_t target);
void game_on_end_turn_pressed(game_state_t *game, linear_allocator_t *allocator);
void game_on_input_event(game_state_t *game, linear_allocator_t *allocator, input_event_t event);
