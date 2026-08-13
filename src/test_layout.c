#include "test_layout.h"
#include "lib/assert.h"
#include "game/layout.h"

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
    assert_test(tx == 0 && ty == 0);

    assert_test(screen_to_grid(v, 319, 199, &tx, &ty));
    assert_test(tx == 15 && ty == 9);

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

const test_case_t g_layout_tests[] = {
    { TEST_NAME("layout_compute_defaults"), test_layout_compute_defaults },
    { TEST_NAME("point_in_rect"), test_point_in_rect },
    { TEST_NAME("screen_to_grid_corners"), test_screen_to_grid_corners },
    { TEST_NAME("grid_to_screen_round_trips_with_screen_to_grid"), test_grid_to_screen_round_trips_with_screen_to_grid },
};

const uint32_t g_layout_tests_count = sizeof(g_layout_tests) / sizeof(g_layout_tests[0]);
