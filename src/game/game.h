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
    slice_t grid_align;
    grid_t grid;
    slice_t entity_list_align;
    slice_entity_t entities;
    slice_t turn_order_align;
    // TODO: this should be included in the turn_state_t
    slice_entity_ptr_t turn_order_capacity; // full allocation turn.order was seeded from; turn.order shrinks as entities die, this doesn't
    turn_state_t turn;
    viewport_t viewport;
    entity_t* selected_entity; // 0 if none
    position_t hover;
    bool hover_valid;
    game_over_t game_over;
} game_state_t;

// Assembles game state from an already-allocated grid, entity list and turn
// order. The caller owns allocation: push grid_align() before grid_init, an
// entity_t alignment before entity_list_init, and an entity_ptr_t alignment
// before turn_order_init (populated with turn_order_add for every entity in
// `entities`), and pass all three markers here so game_deinit can pop
// everything in reverse order.
game_state_t game_init(slice_t grid_align, grid_t grid, slice_t entity_list_align, slice_entity_t entities, slice_t turn_order_align, slice_entity_ptr_t turn_order, int fb_width, int fb_height, int hud_height);

// Pops the grid+entities+turn-order region, including the alignment padding pushed before each.
void game_deinit(linear_allocator_t *allocator, game_state_t state);

void game_on_entity_pressed(game_state_t *game, entity_t* entity);
void game_on_tile_pressed(game_state_t *game, linear_allocator_t *allocator, position_t target);
void game_on_end_turn_pressed(game_state_t *game, linear_allocator_t *allocator);
void game_on_input_event(game_state_t *game, linear_allocator_t *allocator, input_event_t event);
