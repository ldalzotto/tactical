#pragma once

#include "game/game.h"
#include "test_invariants.h"

// Shared layout for game orchestration tests. Matches
// test_layout_compute_defaults, so tile_size=20 and end_turn_button is
// x=250,y=210,w=60,h=20.
#define GAME_TEST_GRID_WIDTH 16
#define GAME_TEST_GRID_HEIGHT 10
#define GAME_TEST_FB_WIDTH 320
#define GAME_TEST_FB_HEIGHT 240
#define GAME_TEST_HUD_HEIGHT 40

// game_on_entity_pressed/game_on_tile_pressed/game_on_end_turn_pressed are
// private to game.c, so drive them via game_on_input_event like real input.
// Sends MOUSE_MOVE before the click, since a real pointer hovers first --
// this stages game->pathing.blast_tiles, which game_cast_attack_area
// asserts is already staged for an AoE cast.
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

// `visible_count` is the number of skill buttons currently drawn (see
// layout_visible_skill_button_count) -- callers know it from the entity
// they set up, since it's no longer implicit in a fixed-size viewport array.
static inline void test_click_skill_button(game_state_t *game, linear_allocator_t *allocator, int index, int visible_count) {
    rect_t button = layout_skill_button_rect(game->viewport, index, visible_count);
    input_event_t click = { .type = INPUT_EVENT_MOUSE_CLICK, .x = button.x + 1, .y = button.y + 1 };
    game_on_input_event(game, allocator, click);
    assert_game_invariants(game);
}

// `key` is the raw char pushed in event.x (see INPUT_EVENT_KEY_DOWN); pass
// '1'..'9' for skill selection, or any other char to exercise the no-op path.
static inline void test_press_key(game_state_t *game, linear_allocator_t *allocator, char key) {
    input_event_t press = { .type = INPUT_EVENT_KEY_DOWN, .x = (int32_t)key, .y = 0 };
    game_on_input_event(game, allocator, press);
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

// Mirrors render.c's reachable-tiles overlay: reachable iff
// walking_distances >= 1 (0 is the mover's own tile).
static inline bool test_position_reachable(game_state_t *game, position_t target) {
    return pathing_distance_at(game->pathing.walking_distances, game->grid, target) >= 1;
}

// Counts reachable tiles; valid only in GAME_MODE_MOVEMENT (otherwise
// walking_distances is an empty marker).
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
