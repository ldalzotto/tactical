#pragma once

#include "game/game.h"

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
// game_on_input_event.
static inline void test_click_tile(game_state_t *game, linear_allocator_t *allocator, position_t target) {
    int px, py;
    grid_to_screen(game->viewport, target.x, target.y, &px, &py);
    input_event_t click = { .type = INPUT_EVENT_MOUSE_CLICK, .x = px + 1, .y = py + 1 };
    game_on_input_event(game, allocator, click);
}

static inline void test_move_to_pixel(game_state_t *game, linear_allocator_t *allocator, int x, int y) {
    input_event_t move = { .type = INPUT_EVENT_MOUSE_MOVE, .x = x, .y = y };
    game_on_input_event(game, allocator, move);
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
}

static inline void test_click_attack_toggle(game_state_t *game, linear_allocator_t *allocator) {
    rect_t button = game->viewport.attack_button;
    input_event_t click = { .type = INPUT_EVENT_MOUSE_CLICK, .x = button.x + 1, .y = button.y + 1 };
    game_on_input_event(game, allocator, click);
}

static inline void test_click_skill_button(game_state_t *game, linear_allocator_t *allocator, int index) {
    rect_t button = SLICE_AT(viewport_skill_buttons(&game->viewport), index);
    input_event_t click = { .type = INPUT_EVENT_MOUSE_CLICK, .x = button.x + 1, .y = button.y + 1 };
    game_on_input_event(game, allocator, click);
}

static inline bool test_tile_list_contains(slice_position_t tiles, position_t target) {
    for (SLICE_FOREACH(tiles, tile_s)) {
        if (position_equals(SLICE_DEREF(tile_s), target)) {
            return true;
        }
    }
    return false;
}
