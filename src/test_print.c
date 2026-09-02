#include "test_print.h"
#include "game/print.h"
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
#include <stdint.h>

// Passing `allocator` itself as print_*'s `dest` (see fmt.h) pushes
// the JSON onto it instead of streaming to the runtime debug bridge.
// Consecutive fmt_write* calls land contiguously, so marking the cursor
// before the call and reading back up to the cursor after it recovers
// exactly what was printed -- no separate capture mechanism needed.

PRIVATE void test_print_position(linear_allocator_t *allocator) {
    void *mark = allocator->cursor;
    print_position(allocator, (position_t){ .x = 3, .y = 4 });
    slice_t captured = { mark, allocator->cursor };

    assert_test(slice_equals(captured, STR("{\"x\":3,\"y\":4}\n")));

    linear_allocator_pop(allocator, captured);
}

PRIVATE void test_print_skill(linear_allocator_t *allocator) {
    void *mark = allocator->cursor;
    print_skill(allocator, SKILL_FIREBALL);
    slice_t captured = { mark, allocator->cursor };

    assert_test(slice_equals(captured, STR("{\"range\":4,\"damage\":4,\"ap_cost\":1,\"aoe_radius\":2}\n")));

    linear_allocator_pop(allocator, captured);
}

PRIVATE void test_print_entity(linear_allocator_t *allocator) {
    game_state_t game = scenario_setup_default(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    void *mark = allocator->cursor;
    print_entity(allocator, SLICE_AT(game.entities, 0));
    slice_t captured = { mark, allocator->cursor };

    assert_test(slice_equals(captured, STR(
        "{\"team\":\"player\",\"pos\":{\"x\":1,\"y\":2},\"hp\":10,\"max_hp\":10,"
        "\"ap\":1,\"max_ap\":1,\"mp\":3,\"max_mp\":3,\"alive\":true,\"skill_count\":2}\n"
    )));

    linear_allocator_pop(allocator, captured);
    game_deinit(allocator, game);
}

PRIVATE void test_print_entity_list(linear_allocator_t *allocator) {
    game_state_t game = scenario_setup_default(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    void *mark = allocator->cursor;
    print_entity_list(allocator, game.entities);
    slice_t captured = { mark, allocator->cursor };

    assert_test(slice_equals(captured, STR(
        "{\"index\":0,\"team\":\"player\",\"pos\":{\"x\":1,\"y\":2},\"hp\":10,\"max_hp\":10,\"ap\":1,\"max_ap\":1,\"mp\":3,\"max_mp\":3,\"alive\":true,\"skill_count\":2}\n"
        "{\"index\":1,\"team\":\"player\",\"pos\":{\"x\":1,\"y\":5},\"hp\":10,\"max_hp\":10,\"ap\":1,\"max_ap\":1,\"mp\":3,\"max_mp\":3,\"alive\":true,\"skill_count\":2}\n"
        "{\"index\":2,\"team\":\"player\",\"pos\":{\"x\":1,\"y\":8},\"hp\":10,\"max_hp\":10,\"ap\":1,\"max_ap\":1,\"mp\":3,\"max_mp\":3,\"alive\":true,\"skill_count\":2}\n"
        "{\"index\":3,\"team\":\"enemy\",\"pos\":{\"x\":14,\"y\":2},\"hp\":10,\"max_hp\":10,\"ap\":1,\"max_ap\":1,\"mp\":3,\"max_mp\":3,\"alive\":true,\"skill_count\":2}\n"
        "{\"index\":4,\"team\":\"enemy\",\"pos\":{\"x\":14,\"y\":5},\"hp\":10,\"max_hp\":10,\"ap\":1,\"max_ap\":1,\"mp\":3,\"max_mp\":3,\"alive\":true,\"skill_count\":2}\n"
        "{\"index\":5,\"team\":\"enemy\",\"pos\":{\"x\":14,\"y\":8},\"hp\":10,\"max_hp\":10,\"ap\":1,\"max_ap\":1,\"mp\":3,\"max_mp\":3,\"alive\":true,\"skill_count\":2}\n"
    )));

    linear_allocator_pop(allocator, captured);
    game_deinit(allocator, game);
}

PRIVATE void test_print_turn_state(linear_allocator_t *allocator) {
    game_state_t game = scenario_setup_default(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    void *mark = allocator->cursor;
    print_turn_state(allocator, game.turn);
    slice_t captured = { mark, allocator->cursor };

    assert_test(slice_equals(captured, STR("{\"cursor\":0,\"order_count\":6}\n")));

    linear_allocator_pop(allocator, captured);
    game_deinit(allocator, game);
}

PRIVATE void test_print_game_state(linear_allocator_t *allocator) {
    game_state_t game = scenario_setup_default(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    void *mark = allocator->cursor;
    print_game_state(allocator, game);
    slice_t captured = { mark, allocator->cursor };

    assert_test(slice_equals(captured, STR(
        "{\"mode\":\"none\",\"hover\":null,\"selected_skill\":0,\"game_over\":\"none\",\"entity_count\":6,\"turn_cursor\":0}\n"
    )));

    linear_allocator_pop(allocator, captured);
    game_deinit(allocator, game);
}

const test_case_t g_print_tests[] = {
    { TEST_NAME("print_position"), test_print_position },
    { TEST_NAME("print_skill"), test_print_skill },
    { TEST_NAME("print_entity"), test_print_entity },
    { TEST_NAME("print_entity_list"), test_print_entity_list },
    { TEST_NAME("print_turn_state"), test_print_turn_state },
    { TEST_NAME("print_game_state"), test_print_game_state },
};

const uint32_t g_print_tests_count = sizeof(g_print_tests) / sizeof(g_print_tests[0]);
