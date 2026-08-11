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
    turn_state_t turn;
    viewport_t viewport;
    entity_t* selected_entity; // 0 if none
    position_t hover;
    bool hover_valid;
    game_over_t game_over;
    linear_allocator_t scratch;        // internal arena for game-owned working data; currently
                                        // just hosts reachable_tiles, but any future per-game
                                        // UI-state buffer can push into it too
    slice_t reachable_align;           // alignment padding pushed into scratch right before
                                        // reachable_tiles, when it's non-empty; zero-length marker
                                        // at the current scratch cursor when it's empty
    slice_position_t reachable_tiles;  // tiles the selected entity can currently reach; length is
                                        // resliced on each recompute to reflect the live count
} game_state_t;

// Assembles game state from an already-allocated grid, entity list and turn
// order. The caller owns allocation: push grid_align() before grid_init, an
// entity_t alignment before entity_list_init, and an entity_ptr_t alignment
// before turn_order_init (populated with turn_order_add for every entity in
// `entities`), and pass all three markers here so game_deinit can pop
// everything in reverse order. `allocator` is also used to carve out the
// game's internal scratch arena (a fixed byte region; position_t alignment
// inside it is handled lazily, only once something is actually pushed).
game_state_t game_init(linear_allocator_t *allocator, slice_t grid_align, grid_t grid, slice_t entity_list_align, slice_entity_t entities, slice_t turn_order_align, slice_entity_ptr_t turn_order, int fb_width, int fb_height, int hud_height);

// Pops the scratch arena, then the grid+entities+turn-order region,
// including the alignment padding pushed before each.
void game_deinit(linear_allocator_t *allocator, game_state_t state);

void game_on_input_event(game_state_t *game, linear_allocator_t *allocator, input_event_t event);
