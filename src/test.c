#include "test.h"
#include "lib/assert.h"
#include "lib/runtime.h"
#include "game/entity.h"
#include "game/grid.h"
#include "game/layout.h"

#define TEST_NAME(str) (slice_t){ .begin = (void *)(str), .end = (void *)((str) + sizeof(str) - 1) }

SLICE_DEFINE(uint8_t);
SLICE_DEFINE(uint32_t);

static void test_pass_example(void) {
    assert_test(1 + 1 == 2);
}

static void test_fail_example(void) {
    expect_panic_begin();
    assert_test(1 + 1 == 3);
    assert_test(expect_panic_end());

    expect_panic_begin();
    assert_test(1 == 2);
    assert_test(expect_panic_end());

    assert_test(1 + 1 == 2);
}

static void test_byteoffset(void) {
    static uint32_t values[4] = { 10, 20, 30, 40 };

    uint32_t *third = typeoffset(values, 2);

    assert_test(*third == 30);
}

static void test_linear_allocator_init(void) {
    static char buffer[16];
    slice_t data = { buffer, buffer + sizeof(buffer) };

    linear_allocator_t allocator = linear_allocator_init(data);

    assert_test(allocator.cursor == data.begin);
    assert_test(allocator.data.begin == data.begin);
    assert_test(allocator.data.end == data.end);
}

static void test_linear_allocator_deinit(void) {
    static char buffer[16];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    linear_allocator_deinit(&allocator);

    assert_test(allocator.cursor == allocator.data.begin);
}

static void test_linear_allocator_deinit_panics_on_leftover_allocation(void) {
    static char buffer[16];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_uint8_t bytes = LINEAR_ALLOCATOR_PUSH(&allocator, bytes, 4);
    (void)bytes;

    expect_panic_begin();
    linear_allocator_deinit(&allocator);
    assert_test(expect_panic_end());
}

static void test_linear_allocator_push(void) {
    static char buffer[16];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_uint8_t bytes = LINEAR_ALLOCATOR_PUSH(&allocator, bytes, 4);

    assert_test(bytes.begin == (uint8_t *)buffer);
    assert_test(SLICE_BYTESIZE(bytes) == 4);
    assert_test(allocator.cursor == bytes.end);

    LINEAR_ALLOCATOR_POP(&allocator, bytes);
}

static void test_linear_allocator_push_panics_on_overflow(void) {
    static char buffer[4];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    expect_panic_begin();
    linear_allocator_push(&allocator, 8);
    assert_test(expect_panic_end());
}

static void test_linear_allocator_push_alignment(void) {
    static _Alignas(8) char buffer[16];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_uint8_t byte = LINEAR_ALLOCATOR_PUSH(&allocator, byte, 1);

    slice_uint32_t witness;
    slice_t padding = LINEAR_ALLOCATOR_PUSH_ALIGNMENT(&allocator, witness);
    assert_test(SLICE_BYTESIZE(padding) == _Alignof(uint32_t) - 1);

    slice_uint32_t value = LINEAR_ALLOCATOR_PUSH(&allocator, value, 1);
    assert_test((uintptr_t)value.begin % _Alignof(uint32_t) == 0);

    LINEAR_ALLOCATOR_POP(&allocator, value);
    linear_allocator_pop(&allocator, padding);
    LINEAR_ALLOCATOR_POP(&allocator, byte);
}

static void test_linear_allocator_push_alignment_panics_on_non_power_of_two_alignment(void) {
    static char buffer[16];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    expect_panic_begin();
    linear_allocator_push_alignment(&allocator, 3);
    assert_test(expect_panic_end());
}

static void test_linear_allocator_pop(void) {
    static char buffer[16];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_uint8_t bytes = LINEAR_ALLOCATOR_PUSH(&allocator, bytes, 8);

    LINEAR_ALLOCATOR_POP(&allocator, bytes);

    assert_test(allocator.cursor == allocator.data.begin);
}

static void test_linear_allocator_pop_panics_on_marker_before_data_begin(void) {
    static char buffer[16];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_t marker = { byteoffset(data.begin, -1), allocator.cursor };

    expect_panic_begin();
    linear_allocator_pop(&allocator, marker);
    assert_test(expect_panic_end());
}

static void test_linear_allocator_pop_panics_on_marker_end_mismatch(void) {
    static char buffer[16];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_uint8_t bytes = LINEAR_ALLOCATOR_PUSH(&allocator, bytes, 8);
    slice_t marker = { bytes.begin, byteoffset(bytes.end, -1) };

    expect_panic_begin();
    linear_allocator_pop(&allocator, marker);
    assert_test(expect_panic_end());
}

static void test_linear_allocator_pop_move(void) {
    static char buffer[64];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_uint8_t a = LINEAR_ALLOCATOR_PUSH(&allocator, a, 4);
    SLICE_AT(a, 0) = 'A'; SLICE_AT(a, 1) = 'A';
    SLICE_AT(a, 2) = 'A'; SLICE_AT(a, 3) = 'A';

    slice_uint8_t b = LINEAR_ALLOCATOR_PUSH(&allocator, b, 4);

    slice_uint8_t c = LINEAR_ALLOCATOR_PUSH(&allocator, c, 4);
    SLICE_AT(c, 0) = 'C'; SLICE_AT(c, 1) = 'C';
    SLICE_AT(c, 2) = 'C'; SLICE_AT(c, 3) = 'C';

    LINEAR_ALLOCATOR_POP_MOVE(&allocator, c, b);

    assert_test(SLICE_AT(b, 0) == 'C');
    assert_test(SLICE_AT(b, 1) == 'C');
    assert_test(SLICE_AT(b, 2) == 'C');
    assert_test(SLICE_AT(b, 3) == 'C');
    assert_test(allocator.cursor == byteoffset(b.begin, 4));

    linear_allocator_pop(&allocator, (slice_t){ b.begin, allocator.cursor });
    linear_allocator_pop(&allocator, a.slice);
}

static void test_linear_allocator_pop_move_panics_on_move_forward(void) {
    static char buffer[64];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_uint8_t a = LINEAR_ALLOCATOR_PUSH(&allocator, a, 4);
    slice_uint8_t b = LINEAR_ALLOCATOR_PUSH(&allocator, b, 4);
    (void)a;

    slice_t to = { byteoffset(b.begin, 4), byteoffset(b.begin, 8) };

    expect_panic_begin();
    linear_allocator_pop_move(&allocator, b.slice, to);
    assert_test(expect_panic_end());
}

static void test_linear_allocator_pop_move_panics_on_from_not_top_of_stack(void) {
    static char buffer[64];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_uint8_t a = LINEAR_ALLOCATOR_PUSH(&allocator, a, 4);
    slice_uint8_t b = LINEAR_ALLOCATOR_PUSH(&allocator, b, 4);
    (void)b;

    slice_t to = { a.begin, a.begin };

    expect_panic_begin();
    linear_allocator_pop_move(&allocator, a.slice, to);
    assert_test(expect_panic_end());
}

static void test_slice_at(void) {
    static uint8_t buffer[4] = { 1, 2, 3, 4 };
    slice_uint8_t s = { .slice = { buffer, buffer + sizeof(buffer) } };

    assert_test(SLICE_AT(s, 0) == 1);
    assert_test(SLICE_AT(s, 2) == 3);

    SLICE_AT(s, 1) = 42;
    assert_test(buffer[1] == 42);
}

static void test_slice_at_panics_on_non_power_of_two_alignment(void) {
    static uint8_t buffer[8];
    slice_t s = { buffer, buffer + sizeof(buffer) };

    expect_panic_begin();
    slice_at(s, 0, 3);
    assert_test(expect_panic_end());
}

static void test_slice_at_panics_on_out_of_bounds(void) {
    static uint8_t buffer[4];
    slice_t s = { buffer, buffer + sizeof(buffer) };

    expect_panic_begin();
    slice_at(s, 8, 1);
    assert_test(expect_panic_end());
}

static void test_slice_at_panics_on_misalignment(void) {
    static _Alignas(4) uint8_t buffer[8];
    slice_t s = { buffer, buffer + sizeof(buffer) };

    expect_panic_begin();
    slice_at(s, 1, 4);
    assert_test(expect_panic_end());
}

static void test_slice_advance(void) {
    static uint8_t buffer[4] = { 1, 2, 3, 4 };
    slice_uint8_t s = { .slice = { buffer, buffer + sizeof(buffer) } };

    slice_uint8_t advanced = SLICE_ADVANCE(s, 2);

    assert_test(advanced.begin == buffer + 2);
    assert_test(advanced.end == s.end);
    assert_test(SLICE_AT(advanced, 0) == 3);
}

static void test_slice_advance_panics_on_out_of_bounds(void) {
    static uint8_t buffer[4];
    slice_uint8_t s = { .slice = { buffer, buffer + sizeof(buffer) } };

    expect_panic_begin();
    (void)SLICE_ADVANCE(s, 8);
    assert_test(expect_panic_end());
}

static void test_bytesize(void) {
    static uint8_t buffer[7];
    slice_uint8_t s = { .slice = { buffer, buffer + sizeof(buffer) } };

    assert_test(SLICE_BYTESIZE(s) == 7);
}

static void test_input_event_layout(void) {
    assert_test(sizeof(input_event_t) == 12);

    static input_event_t events[2];
    slice_input_event_t s = { .slice = { events, events + 2 } };

    SLICE_AT(s, 0) = (input_event_t){ .type = INPUT_EVENT_MOUSE_MOVE, .x = 1, .y = 2 };
    SLICE_AT(s, 1) = (input_event_t){ .type = INPUT_EVENT_MOUSE_CLICK, .x = 3, .y = 4 };

    assert_test(SLICE_AT(s, 0).type == INPUT_EVENT_MOUSE_MOVE);
    assert_test(SLICE_AT(s, 1).x == 3);
    assert_test(SLICE_AT(s, 1).y == 4);
}

static void test_grid_init(void) {
    static _Alignas(tile_t) char buffer[256];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    grid_t grid = grid_init(&allocator, 3, 2);

    assert_test(grid.width == 3);
    assert_test(grid.height == 2);
    assert_test(SLICE_BYTESIZE(grid.tiles) == 6 * (ptrdiff_t)sizeof(tile_t));

    for (int y = 0; y < grid.height; y++) {
        for (int x = 0; x < grid.width; x++) {
            assert_test(grid_is_walkable(grid, x, y));
        }
    }

    grid_deinit(&allocator, grid);
}

static void test_grid_in_bounds(void) {
    static _Alignas(tile_t) char buffer[256];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    grid_t grid = grid_init(&allocator, 4, 3);

    assert_test(grid_in_bounds(grid, 0, 0));
    assert_test(grid_in_bounds(grid, 3, 2));
    assert_test(!grid_in_bounds(grid, 4, 0));
    assert_test(!grid_in_bounds(grid, 0, 3));
    assert_test(!grid_in_bounds(grid, -1, 0));
    assert_test(!grid_in_bounds(grid, 0, -1));

    grid_deinit(&allocator, grid);
}

static void test_grid_set_walkable_round_trip(void) {
    static _Alignas(tile_t) char buffer[256];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    grid_t grid = grid_init(&allocator, 3, 3);

    assert_test(grid_is_walkable(grid, 1, 1));

    grid_set_walkable(grid, 1, 1, false);
    assert_test(!grid_is_walkable(grid, 1, 1));

    grid_set_walkable(grid, 1, 1, true);
    assert_test(grid_is_walkable(grid, 1, 1));

    grid_deinit(&allocator, grid);
}

static void test_grid_tile_at_panics_on_out_of_bounds(void) {
    static _Alignas(tile_t) char buffer[256];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    grid_t grid = grid_init(&allocator, 2, 2);

    // x within a valid row-major flat index range but past the grid's
    // logical width still panics: pick indices whose flat offset (y *
    // width + x) exceeds the total tile count (4), not just the grid's
    // width/height, since grid_tile_at indexes the flat array directly.
    expect_panic_begin();
    grid_tile_at(grid, 5, 0);
    assert_test(expect_panic_end());

    expect_panic_begin();
    grid_tile_at(grid, 0, 100);
    assert_test(expect_panic_end());

    grid_deinit(&allocator, grid);
}

static void test_grid_deinit_right_after_init(void) {
    static _Alignas(tile_t) char buffer[256];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    grid_t grid = grid_init(&allocator, 5, 5);
    grid_deinit(&allocator, grid);

    assert_test(allocator.cursor == allocator.data.begin);
}

static void test_entity_spawn_sequential_ids(void) {
    static _Alignas(entity_t) char buffer[1024];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    entity_list_t list = entity_list_init(&allocator, 4);

    entity_id_t a = entity_spawn(&list, ENTITY_TEAM_PLAYER, 0, 0, 10, 2, 3);
    entity_id_t b = entity_spawn(&list, ENTITY_TEAM_ENEMY, 1, 0, 10, 2, 3);
    entity_id_t c = entity_spawn(&list, ENTITY_TEAM_PLAYER, 2, 0, 10, 2, 3);

    assert_test(a == 0);
    assert_test(b == 1);
    assert_test(c == 2);
    assert_test(list.count == 3);

    entity_list_deinit(&allocator, list);
}

static void test_entity_at_returns_live_mutable_pointer(void) {
    static _Alignas(entity_t) char buffer[1024];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    entity_list_t list = entity_list_init(&allocator, 4);
    entity_id_t id = entity_spawn(&list, ENTITY_TEAM_PLAYER, 5, 5, 10, 2, 3);

    entity_t *entity = entity_at(list, id);
    assert_test(entity->x == 5);
    assert_test(entity->y == 5);
    assert_test(entity->hp == 10);
    assert_test(entity->alive);

    entity->x = 7;
    entity->hp = 3;

    entity_t *reread = entity_at(list, id);
    assert_test(reread->x == 7);
    assert_test(reread->hp == 3);

    entity_list_deinit(&allocator, list);
}

static void test_entity_find_at_ignores_dead_and_empty(void) {
    static _Alignas(entity_t) char buffer[1024];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    entity_list_t list = entity_list_init(&allocator, 4);
    entity_id_t alive_id = entity_spawn(&list, ENTITY_TEAM_PLAYER, 1, 1, 10, 2, 3);
    entity_id_t dead_id = entity_spawn(&list, ENTITY_TEAM_ENEMY, 2, 2, 10, 2, 3);

    entity_damage(list, dead_id, 10);

    assert_test(entity_find_at(list, 1, 1) == alive_id);
    assert_test(entity_find_at(list, 2, 2) == ENTITY_ID_NONE);
    assert_test(entity_find_at(list, 9, 9) == ENTITY_ID_NONE);

    entity_list_deinit(&allocator, list);
}

static void test_entity_damage_clamps_and_flips_alive(void) {
    static _Alignas(entity_t) char buffer[1024];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    entity_list_t list = entity_list_init(&allocator, 4);
    entity_id_t id = entity_spawn(&list, ENTITY_TEAM_PLAYER, 0, 0, 5, 2, 3);

    entity_damage(list, id, 2);
    entity_t *entity = entity_at(list, id);
    assert_test(entity->hp == 3);
    assert_test(entity->alive);

    entity_damage(list, id, 3);
    entity = entity_at(list, id);
    assert_test(entity->hp == 0);
    assert_test(!entity->alive);

    entity_damage(list, id, 5);
    entity = entity_at(list, id);
    assert_test(entity->hp == 0);
    assert_test(!entity->alive);

    entity_list_deinit(&allocator, list);
}

static void test_entity_is_adjacent(void) {
    entity_t center = { .x = 5, .y = 5 };

    entity_t up = { .x = 5, .y = 4 };
    entity_t down = { .x = 5, .y = 6 };
    entity_t left = { .x = 4, .y = 5 };
    entity_t right = { .x = 6, .y = 5 };

    entity_t diag = { .x = 6, .y = 4 };
    entity_t self_pos = { .x = 5, .y = 5 };
    entity_t far = { .x = 8, .y = 5 };

    assert_test(entity_is_adjacent(center, up));
    assert_test(entity_is_adjacent(center, down));
    assert_test(entity_is_adjacent(center, left));
    assert_test(entity_is_adjacent(center, right));

    assert_test(!entity_is_adjacent(center, diag));
    assert_test(!entity_is_adjacent(center, self_pos));
    assert_test(!entity_is_adjacent(center, far));
}

static void test_entity_alive_count_per_team(void) {
    static _Alignas(entity_t) char buffer[1024];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    entity_list_t list = entity_list_init(&allocator, 5);
    entity_id_t p1 = entity_spawn(&list, ENTITY_TEAM_PLAYER, 0, 0, 10, 2, 3);
    entity_spawn(&list, ENTITY_TEAM_PLAYER, 1, 0, 10, 2, 3);
    entity_id_t e1 = entity_spawn(&list, ENTITY_TEAM_ENEMY, 2, 0, 10, 2, 3);
    entity_spawn(&list, ENTITY_TEAM_ENEMY, 3, 0, 10, 2, 3);
    entity_spawn(&list, ENTITY_TEAM_ENEMY, 4, 0, 10, 2, 3);

    entity_damage(list, p1, 10);
    entity_damage(list, e1, 10);

    assert_test(entity_alive_count(list, ENTITY_TEAM_PLAYER) == 1);
    assert_test(entity_alive_count(list, ENTITY_TEAM_ENEMY) == 2);

    entity_list_deinit(&allocator, list);
}

static void test_entity_at_panics_on_out_of_range(void) {
    static _Alignas(entity_t) char buffer[1024];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    entity_list_t list = entity_list_init(&allocator, 4);
    entity_spawn(&list, ENTITY_TEAM_PLAYER, 0, 0, 10, 2, 3);

    expect_panic_begin();
    entity_at(list, 999);
    assert_test(expect_panic_end());

    entity_list_deinit(&allocator, list);
}

static void test_layout_compute_defaults(void) {
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

static void test_point_in_rect(void) {
    rect_t r = { .x = 10, .y = 20, .width = 5, .height = 8 };

    assert_test(point_in_rect(r, 10, 20));
    assert_test(point_in_rect(r, 14, 27));
    assert_test(!point_in_rect(r, 15, 20));
    assert_test(!point_in_rect(r, 10, 28));
    assert_test(!point_in_rect(r, 9, 20));
    assert_test(!point_in_rect(r, 10, 19));
}

static void test_screen_to_grid_corners(void) {
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

static void test_grid_to_screen_round_trips_with_screen_to_grid(void) {
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

static const test_case_t g_tests[] = {
    { TEST_NAME("pass_example"), test_pass_example },
    { TEST_NAME("fail_example"), test_fail_example },
    { TEST_NAME("byteoffset"), test_byteoffset },
    { TEST_NAME("linear_allocator_init"), test_linear_allocator_init },
    { TEST_NAME("linear_allocator_deinit"), test_linear_allocator_deinit },
    { TEST_NAME("linear_allocator_deinit_panics_on_leftover_allocation"), test_linear_allocator_deinit_panics_on_leftover_allocation },
    { TEST_NAME("linear_allocator_push"), test_linear_allocator_push },
    { TEST_NAME("linear_allocator_push_panics_on_overflow"), test_linear_allocator_push_panics_on_overflow },
    { TEST_NAME("linear_allocator_push_alignment"), test_linear_allocator_push_alignment },
    { TEST_NAME("linear_allocator_push_alignment_panics_on_non_power_of_two_alignment"), test_linear_allocator_push_alignment_panics_on_non_power_of_two_alignment },
    { TEST_NAME("linear_allocator_pop"), test_linear_allocator_pop },
    { TEST_NAME("linear_allocator_pop_panics_on_marker_before_data_begin"), test_linear_allocator_pop_panics_on_marker_before_data_begin },
    { TEST_NAME("linear_allocator_pop_panics_on_marker_end_mismatch"), test_linear_allocator_pop_panics_on_marker_end_mismatch },
    { TEST_NAME("linear_allocator_pop_move"), test_linear_allocator_pop_move },
    { TEST_NAME("linear_allocator_pop_move_panics_on_move_forward"), test_linear_allocator_pop_move_panics_on_move_forward },
    { TEST_NAME("linear_allocator_pop_move_panics_on_from_not_top_of_stack"), test_linear_allocator_pop_move_panics_on_from_not_top_of_stack },
    { TEST_NAME("slice_at"), test_slice_at },
    { TEST_NAME("slice_at_panics_on_non_power_of_two_alignment"), test_slice_at_panics_on_non_power_of_two_alignment },
    { TEST_NAME("slice_at_panics_on_out_of_bounds"), test_slice_at_panics_on_out_of_bounds },
    { TEST_NAME("slice_at_panics_on_misalignment"), test_slice_at_panics_on_misalignment },
    { TEST_NAME("slice_advance"), test_slice_advance },
    { TEST_NAME("slice_advance_panics_on_out_of_bounds"), test_slice_advance_panics_on_out_of_bounds },
    { TEST_NAME("bytesize"), test_bytesize },
    { TEST_NAME("input_event_layout"), test_input_event_layout },
    { TEST_NAME("grid_init"), test_grid_init },
    { TEST_NAME("grid_in_bounds"), test_grid_in_bounds },
    { TEST_NAME("grid_set_walkable_round_trip"), test_grid_set_walkable_round_trip },
    { TEST_NAME("grid_tile_at_panics_on_out_of_bounds"), test_grid_tile_at_panics_on_out_of_bounds },
    { TEST_NAME("grid_deinit_right_after_init"), test_grid_deinit_right_after_init },
    { TEST_NAME("entity_spawn_sequential_ids"), test_entity_spawn_sequential_ids },
    { TEST_NAME("entity_at_returns_live_mutable_pointer"), test_entity_at_returns_live_mutable_pointer },
    { TEST_NAME("entity_find_at_ignores_dead_and_empty"), test_entity_find_at_ignores_dead_and_empty },
    { TEST_NAME("entity_damage_clamps_and_flips_alive"), test_entity_damage_clamps_and_flips_alive },
    { TEST_NAME("entity_is_adjacent"), test_entity_is_adjacent },
    { TEST_NAME("entity_alive_count_per_team"), test_entity_alive_count_per_team },
    { TEST_NAME("entity_at_panics_on_out_of_range"), test_entity_at_panics_on_out_of_range },
    { TEST_NAME("layout_compute_defaults"), test_layout_compute_defaults },
    { TEST_NAME("point_in_rect"), test_point_in_rect },
    { TEST_NAME("screen_to_grid_corners"), test_screen_to_grid_corners },
    { TEST_NAME("grid_to_screen_round_trips_with_screen_to_grid"), test_grid_to_screen_round_trips_with_screen_to_grid },
};

#define TEST_COUNT (sizeof(g_tests) / sizeof(g_tests[0]))

__attribute__((export_name("test_discovery_count")))
uint32_t test_discovery_count(void) {
    return TEST_COUNT;
}

__attribute__((export_name("test_discovery_name_begin")))
const char *test_discovery_name_begin(uint32_t index) {
    assert_test(index < TEST_COUNT);
    return (const char *)g_tests[index].name.begin;
}

__attribute__((export_name("test_discovery_name_end")))
const char *test_discovery_name_end(uint32_t index) {
    assert_test(index < TEST_COUNT);
    return (const char *)g_tests[index].name.end;
}

__attribute__((export_name("test_discovery_fn_at")))
test_fn_t test_discovery_fn_at(uint32_t index) {
    assert_test(index < TEST_COUNT);
    return g_tests[index].fn;
}

__attribute__((export_name("test_run")))
void test_run(test_fn_t fn) {
    expect_panic_end();
    fn();
}
