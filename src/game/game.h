#pragma once

#include "../lib/linkage.h"

#include <stdbool.h>

#include "../lib/memory.h"
#include "../lib/runtime.h"
#include "entity.h"
#include "grid.h"
#include "layout.h"
#include "position.h"
#include "pathing_ranges.h"
#include "turn.h"

typedef enum {
    GAME_OVER_NONE = 0,
    GAME_OVER_WIN = 1,
    GAME_OVER_LOSE = 2,
} game_over_t;

// Whether the active entity's pending action, if any, is a move or an
// attack. NONE shows no overlay (render.c gates on mode == MOVEMENT);
// MOVEMENT shows reachable tiles (pathing.walking_distances); ATTACK shows
// pathing.attack_range_tiles.
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
    int selected_skill; // index into active entity's skills; reset to 0 on turn advance
    game_over_t game_over;
    linear_allocator_t scratch;  // game-owned working arena; hosts
                                  // pathing.walking_distances/attack_range_tiles
    pathing_ranges_t pathing;
} game_state_t;

// Assembles game state from already-allocated grid, entity list, skill list,
// and turn order, in that memory order. Caller pushes the alignment marker
// before each region's init (grid_align/entity_t/skill_t/entity_ptr_t) and
// passes all four so game_deinit can pop everything in reverse. `entities`'
// skills must already be populated via skill_list_add. `allocator` also
// carves out the game's scratch arena.
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
