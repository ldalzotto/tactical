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
#include "lib/runtime.h"
#include "test.h"
#include "test_game_helpers.h"
#include <stdint.h>

// debug_capture_begin/end (runtime.h, APP_BUILD_TESTS-only) redirects
// debug_write/debug_flush_line into a buffer instead of the JS bridge, so
// these assert on the exact JSON debug_print_* produces, not just that the
// call ran without panicking.

PRIVATE void test_debug_print_position(linear_allocator_t *allocator) {
    slice_uint8_t buf = LINEAR_ALLOCATOR_PUSH(allocator, buf, 64);

    debug_capture_begin(buf.slice);
    debug_print_position((position_t){ .x = 3, .y = 4 });
    slice_t captured = debug_capture_end();

    assert_test(slice_equals(captured, STR("{\"x\":3,\"y\":4}\n")));

    LINEAR_ALLOCATOR_POP(allocator, buf);
}

PRIVATE void test_debug_print_skill(linear_allocator_t *allocator) {
    slice_uint8_t buf = LINEAR_ALLOCATOR_PUSH(allocator, buf, 96);

    debug_capture_begin(buf.slice);
    debug_print_skill(SKILL_FIREBALL);
    slice_t captured = debug_capture_end();

    assert_test(slice_equals(captured, STR("{\"range\":4,\"damage\":4,\"ap_cost\":1,\"aoe_radius\":2}\n")));

    LINEAR_ALLOCATOR_POP(allocator, buf);
}

PRIVATE void test_debug_print_entity(linear_allocator_t *allocator) {
    game_state_t game = scenario_setup_default(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    slice_uint8_t buf = LINEAR_ALLOCATOR_PUSH(allocator, buf, 256);

    debug_capture_begin(buf.slice);
    debug_print_entity(&SLICE_AT(game.entities, 0));
    slice_t captured = debug_capture_end();

    assert_test(slice_equals(captured, STR(
        "{\"team\":\"player\",\"pos\":{\"x\":1,\"y\":2},\"hp\":10,\"max_hp\":10,"
        "\"ap\":1,\"max_ap\":1,\"mp\":3,\"max_mp\":3,\"alive\":true,\"skill_count\":2}\n"
    )));

    LINEAR_ALLOCATOR_POP(allocator, buf);
    game_deinit(allocator, game);
}

PRIVATE void test_debug_print_entity_list(linear_allocator_t *allocator) {
    game_state_t game = scenario_setup_default(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    slice_uint8_t buf = LINEAR_ALLOCATOR_PUSH(allocator, buf, 1024);

    debug_capture_begin(buf.slice);
    debug_print_entity_list(game.entities);
    slice_t captured = debug_capture_end();

    assert_test(slice_equals(captured, STR(
        "{\"index\":0,\"team\":\"player\",\"pos\":{\"x\":1,\"y\":2},\"hp\":10,\"max_hp\":10,\"ap\":1,\"max_ap\":1,\"mp\":3,\"max_mp\":3,\"alive\":true,\"skill_count\":2}\n"
        "{\"index\":1,\"team\":\"player\",\"pos\":{\"x\":1,\"y\":5},\"hp\":10,\"max_hp\":10,\"ap\":1,\"max_ap\":1,\"mp\":3,\"max_mp\":3,\"alive\":true,\"skill_count\":2}\n"
        "{\"index\":2,\"team\":\"player\",\"pos\":{\"x\":1,\"y\":8},\"hp\":10,\"max_hp\":10,\"ap\":1,\"max_ap\":1,\"mp\":3,\"max_mp\":3,\"alive\":true,\"skill_count\":2}\n"
        "{\"index\":3,\"team\":\"enemy\",\"pos\":{\"x\":14,\"y\":2},\"hp\":10,\"max_hp\":10,\"ap\":1,\"max_ap\":1,\"mp\":3,\"max_mp\":3,\"alive\":true,\"skill_count\":2}\n"
        "{\"index\":4,\"team\":\"enemy\",\"pos\":{\"x\":14,\"y\":5},\"hp\":10,\"max_hp\":10,\"ap\":1,\"max_ap\":1,\"mp\":3,\"max_mp\":3,\"alive\":true,\"skill_count\":2}\n"
        "{\"index\":5,\"team\":\"enemy\",\"pos\":{\"x\":14,\"y\":8},\"hp\":10,\"max_hp\":10,\"ap\":1,\"max_ap\":1,\"mp\":3,\"max_mp\":3,\"alive\":true,\"skill_count\":2}\n"
    )));

    LINEAR_ALLOCATOR_POP(allocator, buf);
    game_deinit(allocator, game);
}

PRIVATE void test_debug_print_turn_state(linear_allocator_t *allocator) {
    game_state_t game = scenario_setup_default(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    slice_uint8_t buf = LINEAR_ALLOCATOR_PUSH(allocator, buf, 64);

    debug_capture_begin(buf.slice);
    debug_print_turn_state(game.turn);
    slice_t captured = debug_capture_end();

    assert_test(slice_equals(captured, STR("{\"cursor\":0,\"order_count\":6}\n")));

    LINEAR_ALLOCATOR_POP(allocator, buf);
    game_deinit(allocator, game);
}

PRIVATE void test_debug_print_game_state(linear_allocator_t *allocator) {
    game_state_t game = scenario_setup_default(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    slice_uint8_t buf = LINEAR_ALLOCATOR_PUSH(allocator, buf, 128);

    debug_capture_begin(buf.slice);
    debug_print_game_state(&game);
    slice_t captured = debug_capture_end();

    assert_test(slice_equals(captured, STR(
        "{\"mode\":\"none\",\"hover\":null,\"selected_skill\":0,\"game_over\":\"none\",\"entity_count\":6,\"turn_cursor\":0}\n"
    )));

    LINEAR_ALLOCATOR_POP(allocator, buf);
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
