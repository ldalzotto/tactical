#pragma once

#include "../lib/linkage.h"

#include <stdbool.h>

#include "../lib/memory.h"
#include "../lib/runtime.h"
#include "entity.h"
#include "grid.h"
#include "layout.h"
#include "position.h"
#include "render_cache.h"
#include "turn.h"

typedef enum {
    GAME_OVER_NONE = 0,
    GAME_OVER_WIN = 1,
    GAME_OVER_LOSE = 2,
} game_over_t;

// Whether the turn's active entity (turn_active_entity) has been selected by
// the player this turn, and if so, whether entity_pressed on an enemy
// attempts an attack instead of a no-op. NONE and MOVEMENT both show
// render.reachable_tiles (NONE leaves it empty); ATTACK shows
// render.attack_range_tiles instead.
typedef enum {
    GAME_MODE_NONE = 0,
    GAME_MODE_MOVEMENT = 1,
    GAME_MODE_ATTACK = 2,
} game_mode_t;

typedef struct {
    slice_t grid_align;
    grid_t grid;
    slice_t entity_list_align;
    slice_entity_t entities;
    slice_t turn_order_align;
    turn_state_t turn;
    viewport_t viewport;
    game_mode_t mode;
    position_t hover;
    bool hover_valid;
    int selected_skill; // index into the active entity's skills, reset to 0 on turn advance
    game_over_t game_over;
    linear_allocator_t scratch;  // internal arena for game-owned working data; hosts
                                  // render.reachable_tiles/attack_range_tiles, and any future
                                  // per-game UI-state buffer
    render_cache_t render;
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
