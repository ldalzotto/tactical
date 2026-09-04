#include "test_game_fuzz.h"
#include "game/entity.h"
#include "game/game.h"
#include "game/position.h"
#include "game/scenario.h"
#include "game/turn.h"
#include "lib/linkage.h"
#include "lib/memory.h"
#include "lib/runtime.h"
#include "test.h"
#include "test_game_helpers.h"
#include "test_invariants.h"
#include <stdint.h>

// Two complementary fuzz drivers over scenario_setup_default's roster, each
// with a fixed xorshift seed so a failure reproduces from the test name alone.
//
// - test_fuzz_pixels: raw input_event_t over the full framebuffer. Exercises
//   pixel-to-grid dispatch and button-hit geometry.
// - test_fuzz_actions: drives semantic actions (click tile, move-hover, end
//   turn, attack toggle, skill select) via test_game_helpers.h, so randomness
//   picks *which* transition happens -- better mode/turn/combat coverage.

PRIVATE void test_fuzz_pixels(linear_allocator_t *allocator, uint32_t seed) {
    game_state_t game = scenario_setup_default(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    for (int i = 0; i < 20000; i++) {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        input_event_t ev = {
            .type = (int32_t)(seed & 1),
            .x = (int32_t)((seed >> 1) % GAME_TEST_FB_WIDTH),
            .y = (int32_t)((seed >> 9) % GAME_TEST_FB_HEIGHT),
        };
        game_on_input_event(&game, allocator, ev);
        assert_game_invariants(&game);
    }

    game_deinit(allocator, game);
}

PRIVATE void test_fuzz_actions(linear_allocator_t *allocator, uint32_t seed) {
    game_state_t game = scenario_setup_default(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    for (int i = 0; i < 20000; i++) {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        position_t target = {
            .x = (int)((seed >> 1) % GAME_TEST_GRID_WIDTH),
            .y = (int)((seed >> 9) % GAME_TEST_GRID_HEIGHT),
        };
        uint32_t action = (seed >> 24) % 5;
        if (action == 0) {
            test_click_tile(&game, allocator, target);
        } else if (action == 1) {
            test_move_tile(&game, allocator, target);
        } else if (action == 2) {
            test_click_end_turn(&game, allocator);
        } else if (action == 3) {
            test_click_attack_toggle(&game, allocator);
        } else {
            int skill_count = entity_skill_count(turn_active_entity(game.turn));
            test_click_skill_button(&game, allocator, (int)(seed % (uint32_t)skill_count), skill_count);
        }
    }

    game_deinit(allocator, game);
}

PRIVATE void test_fuzz_pixels_seed1(linear_allocator_t *allocator) { test_fuzz_pixels(allocator, 0x9E3779B9u); }
PRIVATE void test_fuzz_pixels_seed2(linear_allocator_t *allocator) { test_fuzz_pixels(allocator, 0x1234ABCDu); }
PRIVATE void test_fuzz_pixels_seed3(linear_allocator_t *allocator) { test_fuzz_pixels(allocator, 0xC0FFEE11u); }

PRIVATE void test_fuzz_actions_seed1(linear_allocator_t *allocator) { test_fuzz_actions(allocator, 0x9E3779B9u); }
PRIVATE void test_fuzz_actions_seed2(linear_allocator_t *allocator) { test_fuzz_actions(allocator, 0x1234ABCDu); }
PRIVATE void test_fuzz_actions_seed3(linear_allocator_t *allocator) { test_fuzz_actions(allocator, 0xC0FFEE11u); }

const test_case_t g_game_fuzz_tests[] = {
    { TEST_NAME("game_fuzz_pixels_seed1"), test_fuzz_pixels_seed1 },
    { TEST_NAME("game_fuzz_pixels_seed2"), test_fuzz_pixels_seed2 },
    { TEST_NAME("game_fuzz_pixels_seed3"), test_fuzz_pixels_seed3 },
    { TEST_NAME("game_fuzz_actions_seed1"), test_fuzz_actions_seed1 },
    { TEST_NAME("game_fuzz_actions_seed2"), test_fuzz_actions_seed2 },
    { TEST_NAME("game_fuzz_actions_seed3"), test_fuzz_actions_seed3 },
};

const uint32_t g_game_fuzz_tests_count = sizeof(g_game_fuzz_tests) / sizeof(g_game_fuzz_tests[0]);
