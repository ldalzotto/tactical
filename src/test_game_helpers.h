#pragma once

#include "game/game.h"
#include "test_invariants.h"

// Shared layout used by game orchestration tests: grid_width=16,
// grid_height=10, fb 320x240, hud_height=40 -- same numbers as
// test_layout_compute_defaults, so tile_size=20 and end_turn_button is the
// known rect x=250,y=210,w=60,h=20.
#define GAME_TEST_GRID_WIDTH 16
#define GAME_TEST_GRID_HEIGHT 10
#define GAME_TEST_FB_WIDTH 320
#define GAME_TEST_FB_HEIGHT 240
#define GAME_TEST_HUD_HEIGHT 40

// game_on_entity_pressed/game_on_tile_pressed/game_on_end_turn_pressed are
// private to game.c: drive them the same way real input does, through
// game_on_input_event. A real pointer always hovers a tile before clicking
// it, which is what stages game->pathing.blast_tiles for that tile ahead of
// an AoE cast (game_cast_attack_area asserts it's already staged rather than
// computing it) -- so this sends the matching MOUSE_MOVE first, same as a
// real click would.
static inline void test_click_tile(game_state_t *game, linear_allocator_t *allocator, position_t target) {
    int px, py;
    grid_to_screen(game->viewport, target.x, target.y, &px, &py);
    input_event_t move = { .type = INPUT_EVENT_MOUSE_MOVE, .x = px + 1, .y = py + 1 };
    game_on_input_event(game, allocator, move);
    assert_game_invariants(game);
    input_event_t click = { .type = INPUT_EVENT_MOUSE_CLICK, .x = px + 1, .y = py + 1 };
    game_on_input_event(game, allocator, click);
    assert_game_invariants(game);
}

static inline void test_move_to_pixel(game_state_t *game, linear_allocator_t *allocator, int x, int y) {
    input_event_t move = { .type = INPUT_EVENT_MOUSE_MOVE, .x = x, .y = y };
    game_on_input_event(game, allocator, move);
    assert_game_invariants(game);
}

static inline void test_move_tile(game_state_t *game, linear_allocator_t *allocator, position_t target) {
    int px, py;
    grid_to_screen(game->viewport, target.x, target.y, &px, &py);
    test_move_to_pixel(game, allocator, px + 1, py + 1);
}

static inline void test_click_end_turn(game_state_t *game, linear_allocator_t *allocator) {
    rect_t button = game->viewport.end_turn_button;
    input_event_t click = { .type = INPUT_EVENT_MOUSE_CLICK, .x = button.x + 1, .y = button.y + 1 };
    game_on_input_event(game, allocator, click);
    assert_game_invariants(game);
}

static inline void test_click_attack_toggle(game_state_t *game, linear_allocator_t *allocator) {
    rect_t button = game->viewport.attack_button;
    input_event_t click = { .type = INPUT_EVENT_MOUSE_CLICK, .x = button.x + 1, .y = button.y + 1 };
    game_on_input_event(game, allocator, click);
    assert_game_invariants(game);
}

static inline void test_click_skill_button(game_state_t *game, linear_allocator_t *allocator, int index) {
    rect_t button = SLICE_AT(viewport_skill_buttons(&game->viewport), index);
    input_event_t click = { .type = INPUT_EVENT_MOUSE_CLICK, .x = button.x + 1, .y = button.y + 1 };
    game_on_input_event(game, allocator, click);
    assert_game_invariants(game);
}

static inline bool test_tile_list_contains(slice_position_t tiles, position_t target) {
    for (SLICE_FOREACH(tiles, tile_s)) {
        if (position_equals(SLICE_DEREF(tile_s), target)) {
            return true;
        }
    }
    return false;
}

// Mirrors the reachable-tiles overlay render.c draws: a tile is reachable
// this turn iff its walking_distances entry is >= 1 (0 is the mover's own
// tile, excluded).
static inline bool test_position_reachable(game_state_t *game, position_t target) {
    return pathing_distance_at(game->pathing.walking_distances, game->grid, target) >= 1;
}

// Counts tiles the walking_distances overlay marks reachable this turn --
// only valid to call in GAME_MODE_MOVEMENT (walking_distances is an empty
// marker otherwise).
static inline int test_reachable_tile_count(game_state_t *game) {
    int count = 0;
    for (int ty = 0; ty < game->grid.height; ty++) {
        for (int tx = 0; tx < game->grid.width; tx++) {
            if (test_position_reachable(game, (position_t){tx, ty})) {
                count++;
            }
        }
    }
    return count;
}
