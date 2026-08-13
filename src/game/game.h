#pragma once

#include "../lib/linkage.h"

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

// Data derived purely for rendering, cached so render_frame never has to
// recompute it per frame.
// reachable_tiles and attack_range_tiles are mutually exclusive: exactly one
// of the two is ever populated at a time (the other is kept nullified),
// matching the move/attack toggle -- so scratch only ever holds one live
// tile cache.
typedef struct {
    slice_t reachable_align;           // alignment padding pushed into scratch right before
                                        // reachable_tiles, when it's non-empty; zero-length marker
                                        // at the current scratch cursor when it's empty
    slice_position_t reachable_tiles;  // tiles the selected entity can currently reach; length is
                                        // resliced on each recompute to reflect the live count;
                                        // nullified while attack_mode is on
    slice_t attack_range_align;        // same alignment-marker pattern as reachable_align, but for
                                        // attack_range_tiles; always the topmost region in scratch
    slice_position_t attack_range_tiles; // tiles within the selected entity's skill range, populated
                                          // only while attack_mode is on; nullified otherwise
} game_render_cache_t;

typedef struct {
    slice_t grid_align;
    grid_t grid;
    slice_t entity_list_align;
    slice_entity_t entities;
    slice_t turn_order_align;
    turn_state_t turn;
    viewport_t viewport;
    entity_t* selected_entity; // 0 if none
    bool attack_mode;          // when on, entity_pressed on an enemy attempts an attack instead
                                // of being a no-op; renderer shows render.attack_range_tiles
    position_t hover;
    bool hover_valid;
    game_over_t game_over;
    linear_allocator_t scratch;  // internal arena for game-owned working data; currently
                                  // just hosts render.reachable_tiles, but any future per-game
                                  // UI-state buffer can push into it too
    game_render_cache_t render;
} game_state_t;

// Assembles game state from an already-allocated grid, entity list and turn
// order. The caller owns allocation: push grid_align() before grid_init, an
// entity_t alignment before entity_list_init, and an entity_ptr_t alignment
// before turn_order_init (populated with turn_order_add for every entity in
// `entities`), and pass all three markers here so game_deinit can pop
// everything in reverse order. `allocator` is also used to carve out the
// game's internal scratch arena (a fixed byte region; position_t alignment
// inside it is handled lazily, only once something is actually pushed).
PUBLIC game_state_t game_init(linear_allocator_t *allocator, slice_t grid_align, grid_t grid, slice_t entity_list_align, slice_entity_t entities, slice_t turn_order_align, slice_entity_ptr_t turn_order, int fb_width, int fb_height, int hud_height);

// Pops the scratch arena, then the grid+entities+turn-order region,
// including the alignment padding pushed before each.
PUBLIC void game_deinit(linear_allocator_t *allocator, game_state_t state);

PUBLIC void game_on_input_event(game_state_t *game, linear_allocator_t *allocator, input_event_t event);

#ifdef APP_UNITY_BUILD
#include "game.c"
#endif
