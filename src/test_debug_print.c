#include "test_debug_print.h"
#include "game/debug_print.h"
#include "game/entity.h"
#include "game/game.h"
#include "game/position.h"
#include "game/scenario.h"
#include "game/skill.h"
#include "lib/assert.h"
#include "lib/linkage.h"
#include "lib/memory.h"
#include "test.h"
#include "test_game_helpers.h"
#include <stdbool.h>
#include <stdint.h>

// These are smoke tests: debug_print_* has no return value to assert on,
// so each test's job is to prove the call runs to completion (no panic/trap)
// against real game data, not to inspect the printed text -- there's no
// capture path for what debug_write/debug_log send to the JS side.

PRIVATE void test_debug_print_position(linear_allocator_t *allocator) {
    (void)allocator;
    debug_print_position((position_t){ .x = 3, .y = 4 });
    assert_test(true);
}

PRIVATE void test_debug_print_skill(linear_allocator_t *allocator) {
    (void)allocator;
    debug_print_skill(SKILL_FIREBALL);
    assert_test(true);
}

PRIVATE void test_debug_print_entity(linear_allocator_t *allocator) {
    game_state_t game = scenario_setup_default(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    debug_print_entity(&SLICE_AT(game.entities, 0));
    assert_test(SLICE_TYPESIZE(game.entities) == 6);

    game_deinit(allocator, game);
}

PRIVATE void test_debug_print_entity_list(linear_allocator_t *allocator) {
    game_state_t game = scenario_setup_default(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    debug_print_entity_list(game.entities);
    assert_test(SLICE_TYPESIZE(game.entities) == 6);

    game_deinit(allocator, game);
}

PRIVATE void test_debug_print_turn_state(linear_allocator_t *allocator) {
    game_state_t game = scenario_setup_default(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    debug_print_turn_state(game.turn);
    assert_test(game.turn.cursor == 0);

    game_deinit(allocator, game);
}

PRIVATE void test_debug_print_game_state(linear_allocator_t *allocator) {
    game_state_t game = scenario_setup_default(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    debug_print_game_state(&game);
    assert_test(game.game_over == GAME_OVER_NONE);

    game_deinit(allocator, game);
}

const test_case_t g_debug_print_tests[] = {
    { TEST_NAME("debug_print_position"), test_debug_print_position },
    { TEST_NAME("debug_print_skill"), test_debug_print_skill },
    { TEST_NAME("debug_print_entity"), test_debug_print_entity },
    { TEST_NAME("debug_print_entity_list"), test_debug_print_entity_list },
    { TEST_NAME("debug_print_turn_state"), test_debug_print_turn_state },
    { TEST_NAME("debug_print_game_state"), test_debug_print_game_state },
};

const uint32_t g_debug_print_tests_count = sizeof(g_debug_print_tests) / sizeof(g_debug_print_tests[0]);
