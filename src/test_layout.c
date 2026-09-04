#include "test_layout.h"
#include "game/ui.h"
#include "lib/assert.h"
#include "game/layout.h"
#include "lib/linkage.h"
#include "lib/memory.h"
#include "test.h"
#include <stdint.h>

PRIVATE void test_layout_compute_defaults(linear_allocator_t *allocator) {
    (void)allocator;
    viewport_t v = layout_compute(320, 240, 16, 10, 40);

    assert_test(v.origin_x == 0);
    assert_test(v.origin_y == 0);
    assert_test(v.tile_size == 20);
    assert_test(v.grid_width == 16);
    assert_test(v.grid_height == 10);

    assert_test(v.hud_rect.x == 0);
    assert_test(v.hud_rect.y == 200);
    assert_test(v.hud_rect.width == 320);
    assert_test(v.hud_rect.height == 40);

    assert_test(v.end_turn_button.x == 250);
    assert_test(v.end_turn_button.y == 210);
    assert_test(v.end_turn_button.width == 60);
    assert_test(v.end_turn_button.height == 20);

    assert_test(v.attack_button.x == 180);
    assert_test(v.attack_button.y == 210);
    assert_test(v.attack_button.width == 60);
    assert_test(v.attack_button.height == 20);
}

PRIVATE void test_point_in_rect(linear_allocator_t *allocator) {
    (void)allocator;
    rect_t r = { .x = 10, .y = 20, .width = 5, .height = 8 };

    assert_test(point_in_rect(r, 10, 20));
    assert_test(point_in_rect(r, 14, 27));
    assert_test(!point_in_rect(r, 15, 20));
    assert_test(!point_in_rect(r, 10, 28));
    assert_test(!point_in_rect(r, 9, 20));
    assert_test(!point_in_rect(r, 10, 19));
}

PRIVATE void test_screen_to_grid_corners(linear_allocator_t *allocator) {
    (void)allocator;
    viewport_t v = layout_compute(320, 240, 16, 10, 40);

    int tx, ty;

    assert_test(screen_to_grid(v, 0, 0, &tx, &ty));
    assert_test(tx == 0);
    assert_test(ty == 0);

    assert_test(screen_to_grid(v, 319, 199, &tx, &ty));
    assert_test(tx == 15);
    assert_test(ty == 9);

    assert_test(!screen_to_grid(v, 320, 0, &tx, &ty));
    assert_test(!screen_to_grid(v, 0, 240, &tx, &ty));

    assert_test(!screen_to_grid(v, v.hud_rect.x + 1, v.hud_rect.y + 1, &tx, &ty));
}

PRIVATE void test_grid_to_screen_round_trips_with_screen_to_grid(linear_allocator_t *allocator) {
    (void)allocator;
    viewport_t v = layout_compute(320, 240, 16, 10, 40);

    for (int ty = 0; ty < v.grid_height; ty++) {
        for (int tx = 0; tx < v.grid_width; tx++) {
            int px, py;
            grid_to_screen(v, tx, ty, &px, &py);

            int out_tx, out_ty;
            assert_test(screen_to_grid(v, px, py, &out_tx, &out_ty));
            assert_test(out_tx == tx);
            assert_test(out_ty == ty);
        }
    }
}

// visible_count == SKILL_BAR_WIDTH_BUDGET_SLOTS (2): button width matches
// end_turn_button/attack_button's fixed width exactly -- the reference
// case the shrink-to-fit budget is calibrated against.
PRIVATE void test_layout_skill_button_rect_at_budget_matches_reference_width(linear_allocator_t *allocator) {
    (void)allocator;
    viewport_t v = layout_compute(320, 240, 16, 10, 40);

    rect_t button0 = layout_skill_button_rect(v, 0, 2);
    rect_t button1 = layout_skill_button_rect(v, 1, 2);

    assert_test(button0.width == v.attack_button.width);
    assert_test(button1.width == v.attack_button.width);
    assert_test(button0.height == v.attack_button.height);
    // button0 sits immediately left of attack_button; button1 left of that.
    assert_test(button0.x + button0.width < v.attack_button.x);
    assert_test(button1.x + button1.width <= button0.x);
}

// More buttons than the budget's reference count: width shrinks below the
// fixed button_width rather than the row growing past its pixel budget.
PRIVATE void test_layout_skill_button_rect_shrinks_past_budget(linear_allocator_t *allocator) {
    (void)allocator;
    viewport_t v = layout_compute(320, 240, 16, 10, 40);

    rect_t two = layout_skill_button_rect(v, 0, 2);
    rect_t four = layout_skill_button_rect(v, 0, 4);

    assert_test(four.width < two.width);
    assert_test(four.width > 0);
}

// Enough buttons to push shrink-to-fit below SKILL_BUTTON_MIN_WIDTH: width
// clamps at the floor instead of going to zero/negative.
PRIVATE void test_layout_skill_button_rect_clamps_at_minimum_width(linear_allocator_t *allocator) {
    (void)allocator;
    viewport_t v = layout_compute(320, 240, 16, 10, 40);

    rect_t button = layout_skill_button_rect(v, 0, 11);

    assert_test(button.width == SKILL_BUTTON_MIN_WIDTH);
}

// A3: layout_visible_skill_button_count has no upper clamp -- large skill
// counts are all fully visible/selectable.
PRIVATE void test_layout_visible_skill_button_count_has_no_cap(linear_allocator_t *allocator) {
    (void)allocator;
    assert_test(layout_visible_skill_button_count(true, true, 3) == 3);
    assert_test(layout_visible_skill_button_count(true, true, 9) == 9);
    assert_test(layout_visible_skill_button_count(true, true, 50) == 50);
}

const test_case_t g_layout_tests[] = {
    { TEST_NAME("layout_compute_defaults"), test_layout_compute_defaults },
    { TEST_NAME("point_in_rect"), test_point_in_rect },
    { TEST_NAME("screen_to_grid_corners"), test_screen_to_grid_corners },
    { TEST_NAME("grid_to_screen_round_trips_with_screen_to_grid"), test_grid_to_screen_round_trips_with_screen_to_grid },
    { TEST_NAME("layout_skill_button_rect_at_budget_matches_reference_width"), test_layout_skill_button_rect_at_budget_matches_reference_width },
    { TEST_NAME("layout_skill_button_rect_shrinks_past_budget"), test_layout_skill_button_rect_shrinks_past_budget },
    { TEST_NAME("layout_skill_button_rect_clamps_at_minimum_width"), test_layout_skill_button_rect_clamps_at_minimum_width },
    { TEST_NAME("layout_visible_skill_button_count_has_no_cap"), test_layout_visible_skill_button_count_has_no_cap },
};

const uint32_t g_layout_tests_count = sizeof(g_layout_tests) / sizeof(g_layout_tests[0]);
