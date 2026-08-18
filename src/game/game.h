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
    slice_t skill_list_align;
    slice_skill_t skills;
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

// Assembles game state from already-allocated grid, entity list, skill list
// and turn order, in that memory order. Caller pushes the alignment marker
// before each region's init (grid_align/entity_t/skill_t/entity_ptr_t) and
// passes all four here so game_deinit can pop everything in reverse.
// `entities`' skills must already be populated via skill_list_add and
// assigned into each entity_t.skills. `allocator` also carves out the game's
// internal scratch arena.
PUBLIC game_state_t game_init(linear_allocator_t *allocator, slice_t grid_align, grid_t grid, slice_t entity_list_align, slice_entity_t entities, slice_t skill_list_align, slice_skill_t skills, slice_t turn_order_align, slice_entity_ptr_t turn_order, int fb_width, int fb_height, int hud_height);

// Pops the scratch arena, then the grid+entities+skills+turn-order region,
// including the alignment padding pushed before each.
PUBLIC void game_deinit(linear_allocator_t *allocator, game_state_t state);

// Returns the byte shift game->scratch's growth applied (0 if none), so
// app.c can rebase anything it holds above it (see app_dispatch_input_events).
PUBLIC ptrdiff_t game_on_input_event(game_state_t *game, linear_allocator_t *allocator, input_event_t event);

#ifdef APP_UNITY_BUILD
#include "game.c"
#endif
