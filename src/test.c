#include "test.h"
#include "lib/assert.h"
#include "lib/runtime.h"
#include "game/action.h"
#include "game/ai.h"
#include "game/entity.h"
#include "game/game.h"
#include "game/grid.h"
#include "game/layout.h"
#include "game/pathing.h"
#include "game/scenario.h"
#include "game/turn.h"
#include "game/ui.h"

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
    static char buffer[256];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    grid_t grid = grid_init(&allocator, 3, 2);

    assert_test(grid.width == 3);
    assert_test(grid.height == 2);

    for (int y = 0; y < grid.height; y++) {
        for (int x = 0; x < grid.width; x++) {
            assert_test(grid_is_walkable(grid, (position_t){x, y}));
        }
    }

    grid_deinit(&allocator, grid);
}

static void test_grid_in_bounds(void) {
    static char buffer[256];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    grid_t grid = grid_init(&allocator, 4, 3);

    assert_test(grid_in_bounds(grid, (position_t){0, 0}));
    assert_test(grid_in_bounds(grid, (position_t){3, 2}));
    assert_test(!grid_in_bounds(grid, (position_t){4, 0}));
    assert_test(!grid_in_bounds(grid, (position_t){0, 3}));
    assert_test(!grid_in_bounds(grid, (position_t){-1, 0}));
    assert_test(!grid_in_bounds(grid, (position_t){0, -1}));

    grid_deinit(&allocator, grid);
}

static void test_grid_set_walkable_round_trip(void) {
    static char buffer[256];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    grid_t grid = grid_init(&allocator, 3, 3);

    assert_test(grid_is_walkable(grid, (position_t){1, 1}));

    grid_set_walkable(grid, (position_t){1, 1}, false);
    assert_test(!grid_is_walkable(grid, (position_t){1, 1}));

    grid_set_walkable(grid, (position_t){1, 1}, true);
    assert_test(grid_is_walkable(grid, (position_t){1, 1}));

    grid_deinit(&allocator, grid);
}

static void test_grid_tile_at_panics_on_out_of_bounds(void) {
    static char buffer[256];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    grid_t grid = grid_init(&allocator, 2, 2);

    // x within a valid row-major flat index range but past the grid's
    // logical width still panics: pick indices whose flat offset (y *
    // width + x) exceeds the total tile count (4), not just the grid's
    // width/height, since grid_tile_at indexes the flat array directly.
    expect_panic_begin();
    grid_is_walkable(grid, (position_t){5, 0});
    assert_test(expect_panic_end());

    expect_panic_begin();
    grid_is_walkable(grid, (position_t){0, 100});
    assert_test(expect_panic_end());

    grid_deinit(&allocator, grid);
}

static void test_grid_deinit_right_after_init(void) {
    static char buffer[256];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    grid_t grid = grid_init(&allocator, 5, 5);
    grid_deinit(&allocator, grid);

    assert_test(allocator.cursor == allocator.data.begin);
}

static void test_entity_spawn_sequential_ids(void) {
    static char buffer[1024];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_entity_t list;
    slice_t entity_align = LINEAR_ALLOCATOR_PUSH_ALIGNMENT(&allocator, list);
    list = entity_list_init(&allocator);

    entity_t* a = entity_spawn(&allocator, &list, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* b = entity_spawn(&allocator, &list, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 10, 2, 3);
    entity_t* c = entity_spawn(&allocator, &list, ENTITY_TEAM_PLAYER, (position_t){2, 0}, 10, 2, 3);

    assert_test(typesize(a, a) == 0);
    assert_test(typesize(a, b) == 1);
    assert_test(typesize(a, c) == 2);

    entity_list_deinit(&allocator, list);
    linear_allocator_pop(&allocator, entity_align);
}

static void test_entity_find_at_ignores_dead_and_empty(void) {
    static char buffer[1024];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_entity_t list;
    slice_t entity_align = LINEAR_ALLOCATOR_PUSH_ALIGNMENT(&allocator, list);
    list = entity_list_init(&allocator);
    entity_t* alive = entity_spawn(&allocator, &list, ENTITY_TEAM_PLAYER, (position_t){1, 1}, 10, 2, 3);
    entity_t* dead = entity_spawn(&allocator, &list, ENTITY_TEAM_ENEMY, (position_t){2, 2}, 10, 2, 3);

    entity_damage(dead, 10);

    assert_test(entity_find_at(list, (position_t){1, 1}) == alive);
    assert_test(entity_find_at(list, (position_t){2, 2}) == 0);
    assert_test(entity_find_at(list, (position_t){9, 9}) == 0);

    entity_list_deinit(&allocator, list);
    linear_allocator_pop(&allocator, entity_align);
}

static void test_entity_damage_clamps_and_flips_alive(void) {
    static char buffer[1024];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_entity_t list;
    slice_t entity_align = LINEAR_ALLOCATOR_PUSH_ALIGNMENT(&allocator, list);
    list = entity_list_init(&allocator);
    entity_t* entity = entity_spawn(&allocator, &list, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 5, 2, 3);

    entity_damage(entity, 2);
    assert_test(entity->hp == 3);
    assert_test(entity->alive);

    entity_damage(entity, 3);

    assert_test(entity->hp == 0);
    assert_test(!entity->alive);

    entity_damage(entity, 5);

    assert_test(entity->hp == 0);
    assert_test(!entity->alive);

    entity_list_deinit(&allocator, list);
    linear_allocator_pop(&allocator, entity_align);
}

static void test_entity_is_adjacent(void) {
    entity_t center = { .position = { 5, 5 } };

    entity_t up = { .position = { 5, 4 } };
    entity_t down = { .position = { 5, 6 } };
    entity_t left = { .position = { 4, 5 } };
    entity_t right = { .position = { 6, 5 } };

    entity_t diag = { .position = { 6, 4 } };
    entity_t self_pos = { .position = { 5, 5 } };
    entity_t far = { .position = { 8, 5 } };

    assert_test(entity_is_adjacent(center, up));
    assert_test(entity_is_adjacent(center, down));
    assert_test(entity_is_adjacent(center, left));
    assert_test(entity_is_adjacent(center, right));

    assert_test(!entity_is_adjacent(center, diag));
    assert_test(!entity_is_adjacent(center, self_pos));
    assert_test(!entity_is_adjacent(center, far));
}

static void test_entity_alive_count_per_team(void) {
    static char buffer[1024];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_entity_t list;
    slice_t entity_align = LINEAR_ALLOCATOR_PUSH_ALIGNMENT(&allocator, list);
    list = entity_list_init(&allocator);
    entity_t* p1 = entity_spawn(&allocator, &list, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_spawn(&allocator, &list, ENTITY_TEAM_PLAYER, (position_t){1, 0}, 10, 2, 3);
    entity_t* e1 = entity_spawn(&allocator, &list, ENTITY_TEAM_ENEMY, (position_t){2, 0}, 10, 2, 3);
    entity_spawn(&allocator, &list, ENTITY_TEAM_ENEMY, (position_t){3, 0}, 10, 2, 3);
    entity_spawn(&allocator, &list, ENTITY_TEAM_ENEMY, (position_t){4, 0}, 10, 2, 3);

    entity_damage(p1, 10);
    entity_damage(e1, 10);

    assert_test(entity_alive_count(list, ENTITY_TEAM_PLAYER) == 1);
    assert_test(entity_alive_count(list, ENTITY_TEAM_ENEMY) == 2);

    entity_list_deinit(&allocator, list);
    linear_allocator_pop(&allocator, entity_align);
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

static void test_pathing_flat_grid_distance(void) {
    static char buffer[4096];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    grid_t grid = grid_init(&allocator, 4, 4);

    slice_entity_t entities;
    slice_t entity_align = LINEAR_ALLOCATOR_PUSH_ALIGNMENT(&allocator, entities);
    entities = entity_list_init(&allocator);

    pathing_state_t pathing = pathing_compute_distances(&allocator, grid, entities, 0, (position_t){0, 0}, 100);

    assert_test(pathing_distance_at(pathing, grid, (position_t){0, 0}) == 0);
    assert_test(pathing_distance_at(pathing, grid, (position_t){3, 3}) == 6);

    pathing_deinit(&allocator, pathing);
    entity_list_deinit(&allocator, entities);
    linear_allocator_pop(&allocator, entity_align);
    grid_deinit(&allocator, grid);
}

static void test_pathing_obstacle_forces_detour(void) {
    static char buffer[4096];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    grid_t grid = grid_init(&allocator, 5, 3);

    slice_entity_t entities;
    slice_t entity_align = LINEAR_ALLOCATOR_PUSH_ALIGNMENT(&allocator, entities);
    entities = entity_list_init(&allocator);

    pathing_state_t pathing = pathing_compute_distances(&allocator, grid, entities, 0, (position_t){0, 1}, 100);
    int baseline = pathing_distance_at(pathing, grid, (position_t){4, 1});
    assert_test(baseline == 4);

    pathing_deinit(&allocator, pathing);

    grid_set_walkable(grid, (position_t){2, 0}, false);
    grid_set_walkable(grid, (position_t){2, 1}, false);

    pathing = pathing_compute_distances(&allocator, grid, entities, 0, (position_t){0, 1}, 100);
    int obstructed = pathing_distance_at(pathing, grid, (position_t){4, 1});

    assert_test(obstructed > baseline);

    pathing_deinit(&allocator, pathing);
    entity_list_deinit(&allocator, entities);
    linear_allocator_pop(&allocator, entity_align);
    grid_deinit(&allocator, grid);
}

static void test_pathing_occupied_tile_blocks_corridor(void) {
    static char buffer[4096];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    grid_t grid = grid_init(&allocator, 5, 1);

    slice_entity_t entities;
    slice_t entity_align = LINEAR_ALLOCATOR_PUSH_ALIGNMENT(&allocator, entities);
    entities = entity_list_init(&allocator);
    entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){2, 0}, 10, 2, 3);

    pathing_state_t pathing = pathing_compute_distances(&allocator, grid, entities, 0, (position_t){0, 0}, 100);

    assert_test(pathing_distance_at(pathing, grid, (position_t){1, 0}) == 1);
    assert_test(pathing_distance_at(pathing, grid, (position_t){2, 0}) == -1);
    assert_test(pathing_distance_at(pathing, grid, (position_t){3, 0}) == -1);
    assert_test(pathing_distance_at(pathing, grid, (position_t){4, 0}) == -1);

    pathing_deinit(&allocator, pathing);
    entity_list_deinit(&allocator, entities);
    linear_allocator_pop(&allocator, entity_align);
    grid_deinit(&allocator, grid);
}

static void test_pathing_skip_entity_allows_own_start_tile(void) {
    static char buffer[4096];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    grid_t grid = grid_init(&allocator, 4, 4);

    slice_entity_t entities;
    slice_t entity_align = LINEAR_ALLOCATOR_PUSH_ALIGNMENT(&allocator, entities);
    entities = entity_list_init(&allocator);
    entity_t* mover = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){1, 1}, 10, 2, 3);

    pathing_state_t pathing = pathing_compute_distances(&allocator, grid, entities, mover, (position_t){1, 1}, 100);

    assert_test(pathing_distance_at(pathing, grid, (position_t){1, 1}) == 0);
    assert_test(pathing_distance_at(pathing, grid, (position_t){0, 0}) == 2);

    pathing_deinit(&allocator, pathing);
    entity_list_deinit(&allocator, entities);
    linear_allocator_pop(&allocator, entity_align);
    grid_deinit(&allocator, grid);
}

static void test_pathing_max_steps_caps_distance(void) {
    static char buffer[4096];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    grid_t grid = grid_init(&allocator, 4, 4);

    slice_entity_t entities;
    slice_t entity_align = LINEAR_ALLOCATOR_PUSH_ALIGNMENT(&allocator, entities);
    entities = entity_list_init(&allocator);

    pathing_state_t pathing = pathing_compute_distances(&allocator, grid, entities, 0, (position_t){0, 0}, 2);

    assert_test(pathing_distance_at(pathing, grid, (position_t){2, 0}) == 2);
    assert_test(pathing_distance_at(pathing, grid, (position_t){3, 0}) == -1);
    assert_test(pathing_distance_at(pathing, grid, (position_t){3, 3}) == -1);

    pathing_deinit(&allocator, pathing);
    entity_list_deinit(&allocator, entities);
    linear_allocator_pop(&allocator, entity_align);
    grid_deinit(&allocator, grid);
}

static void test_turn_init_starts_player_turn_one(void) {
    turn_state_t state = turn_init();

    assert_test(state.phase == TURN_PHASE_PLAYER);
    assert_test(state.turn_number == 1);
}

static void test_turn_reset_team_points_ignores_dead_entities(void) {
    static char buffer[1024];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_entity_t list;
    slice_t entity_align = LINEAR_ALLOCATOR_PUSH_ALIGNMENT(&allocator, list);
    list = entity_list_init(&allocator);
    entity_t* alive_id = entity_spawn(&allocator, &list, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* dead_id = entity_spawn(&allocator, &list, ENTITY_TEAM_PLAYER, (position_t){1, 0}, 10, 2, 3);

    entity_damage(dead_id, 10);

    entity_t *alive = alive_id;
    entity_t *dead = dead_id;
    alive->ap = 0;
    alive->mp = 0;
    dead->ap = 0;
    dead->mp = 0;

    turn_reset_team_points(list, ENTITY_TEAM_PLAYER);

    alive = alive_id;
    dead = dead_id;

    assert_test(alive->ap == alive->max_ap);
    assert_test(alive->mp == alive->max_mp);

    assert_test(dead->ap == 0);
    assert_test(dead->mp == 0);

    entity_list_deinit(&allocator, list);
    linear_allocator_pop(&allocator, entity_align);
}

static void test_turn_reset_team_points_only_affects_matching_team(void) {
    static char buffer[1024];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_entity_t list;
    slice_t entity_align = LINEAR_ALLOCATOR_PUSH_ALIGNMENT(&allocator, list);
    list = entity_list_init(&allocator);
    entity_t* player_id = entity_spawn(&allocator, &list, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* enemy_id = entity_spawn(&allocator, &list, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 10, 2, 3);

    player_id->ap = 0;
    player_id->mp = 0;
    enemy_id->ap = 0;
    enemy_id->mp = 0;

    turn_reset_team_points(list, ENTITY_TEAM_PLAYER);

    entity_t *player = player_id;
    entity_t *enemy = enemy_id;

    assert_test(player->ap == player->max_ap);
    assert_test(player->mp == player->max_mp);
    assert_test(enemy->ap == 0);
    assert_test(enemy->mp == 0);

    entity_list_deinit(&allocator, list);
    linear_allocator_pop(&allocator, entity_align);
}

static void test_turn_begin_enemy_phase_resets_only_enemy_team(void) {
    static char buffer[1024];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_entity_t list;
    slice_t entity_align = LINEAR_ALLOCATOR_PUSH_ALIGNMENT(&allocator, list);
    list = entity_list_init(&allocator);
    entity_t* player_id = entity_spawn(&allocator, &list, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* enemy_id = entity_spawn(&allocator, &list, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 10, 2, 3);

    player_id->ap = 0;
    player_id->mp = 0;
    enemy_id->ap = 0;
    enemy_id->mp = 0;

    turn_state_t state = turn_init();
    state = turn_begin_enemy_phase(state, list);

    assert_test(state.phase == TURN_PHASE_ENEMY);
    assert_test(state.turn_number == 1);

    entity_t *player = player_id;
    entity_t *enemy = enemy_id;

    assert_test(player->ap == 0);
    assert_test(player->mp == 0);
    assert_test(enemy->ap == enemy->max_ap);
    assert_test(enemy->mp == enemy->max_mp);

    entity_list_deinit(&allocator, list);
    linear_allocator_pop(&allocator, entity_align);
}

static void test_turn_begin_player_phase_resets_only_player_team_and_increments_turn(void) {
    static char buffer[1024];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_entity_t list;
    slice_t entity_align = LINEAR_ALLOCATOR_PUSH_ALIGNMENT(&allocator, list);
    list = entity_list_init(&allocator);
    entity_t* player_id = entity_spawn(&allocator, &list, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* enemy_id = entity_spawn(&allocator, &list, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 10, 2, 3);

    turn_state_t state = turn_init();
    state = turn_begin_enemy_phase(state, list);

    player_id->ap = 0;
    player_id->mp = 0;
    enemy_id->ap = 0;
    enemy_id->mp = 0;

    state = turn_begin_player_phase(state, list);

    assert_test(state.phase == TURN_PHASE_PLAYER);
    assert_test(state.turn_number == 2);

    entity_t *player = player_id;
    entity_t *enemy = enemy_id;

    assert_test(player->ap == player->max_ap);
    assert_test(player->mp == player->max_mp);
    assert_test(enemy->ap == 0);
    assert_test(enemy->mp == 0);

    entity_list_deinit(&allocator, list);
    linear_allocator_pop(&allocator, entity_align);
}

static void test_action_try_move_succeeds_within_mp(void) {
    static char buffer[4096];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    grid_t grid = grid_init(&allocator, 4, 4);

    slice_entity_t entities;
    slice_t entity_align = LINEAR_ALLOCATOR_PUSH_ALIGNMENT(&allocator, entities);
    entities = entity_list_init(&allocator);
    entity_t* mover = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);

    bool result = action_try_move(&allocator, grid, entities, mover, (position_t){2, 0});
    assert_test(result);

    entity_t *entity = mover;
    assert_test(entity->position.x == 2);
    assert_test(entity->position.y == 0);
    assert_test(entity->mp == 1);

    entity_list_deinit(&allocator, entities);
    linear_allocator_pop(&allocator, entity_align);
    grid_deinit(&allocator, grid);
}

static void test_action_try_move_fails_beyond_mp(void) {
    static char buffer[4096];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    grid_t grid = grid_init(&allocator, 4, 4);

    slice_entity_t entities;
    slice_t entity_align = LINEAR_ALLOCATOR_PUSH_ALIGNMENT(&allocator, entities);
    entities = entity_list_init(&allocator);
    entity_t* mover = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 2);

    bool result = action_try_move(&allocator, grid, entities, mover, (position_t){3, 0});
    assert_test(!result);

    entity_t *entity = mover;
    assert_test(entity->position.x == 0);
    assert_test(entity->position.y == 0);
    assert_test(entity->mp == 2);

    entity_list_deinit(&allocator, entities);
    linear_allocator_pop(&allocator, entity_align);
    grid_deinit(&allocator, grid);
}

static void test_action_try_move_fails_onto_obstacle(void) {
    static char buffer[4096];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    grid_t grid = grid_init(&allocator, 4, 4);
    grid_set_walkable(grid, (position_t){1, 0}, false);

    slice_entity_t entities;
    slice_t entity_align = LINEAR_ALLOCATOR_PUSH_ALIGNMENT(&allocator, entities);
    entities = entity_list_init(&allocator);
    entity_t* mover = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);

    bool result = action_try_move(&allocator, grid, entities, mover, (position_t){1, 0});
    assert_test(!result);

    entity_t *entity = mover;
    assert_test(entity->position.x == 0);
    assert_test(entity->position.y == 0);
    assert_test(entity->mp == 3);

    entity_list_deinit(&allocator, entities);
    linear_allocator_pop(&allocator, entity_align);
    grid_deinit(&allocator, grid);
}

static void test_action_try_move_fails_onto_occupied_tile(void) {
    static char buffer[4096];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    grid_t grid = grid_init(&allocator, 4, 4);

    slice_entity_t entities;
    slice_t entity_align = LINEAR_ALLOCATOR_PUSH_ALIGNMENT(&allocator, entities);
    entities = entity_list_init(&allocator);
    entity_t* mover = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 10, 2, 3);

    bool result = action_try_move(&allocator, grid, entities, mover, (position_t){1, 0});
    assert_test(!result);

    entity_t *entity = mover;
    assert_test(entity->position.x == 0);
    assert_test(entity->position.y == 0);
    assert_test(entity->mp == 3);

    entity_list_deinit(&allocator, entities);
    linear_allocator_pop(&allocator, entity_align);
    grid_deinit(&allocator, grid);
}

static void test_action_try_attack_succeeds_on_adjacent_enemy(void) {
    static char buffer[1024];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_entity_t entities = entity_list_init(&allocator);
    entity_t* attacker = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* defender = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 10, 2, 3);

    bool result = action_try_attack(attacker, defender);
    assert_test(result);

    entity_t *attacker_entity = attacker;
    entity_t *defender_entity = defender;

    assert_test(attacker_entity->ap == 1);
    assert_test(defender_entity->hp == 5);
    assert_test(defender_entity->alive);

    entity_list_deinit(&allocator, entities);
}

static void test_action_try_attack_killing_blow_leaves_defender_dead(void) {
    static char buffer[1024];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_entity_t entities = entity_list_init(&allocator);
    entity_t* attacker = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* defender = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 3, 2, 3);

    bool result = action_try_attack(attacker, defender);
    assert_test(result);

    entity_t *defender_entity = defender;
    assert_test(defender_entity->hp == 0);
    assert_test(!defender_entity->alive);

    entity_list_deinit(&allocator, entities);
}

static void test_action_try_attack_fails_when_not_adjacent(void) {
    static char buffer[1024];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_entity_t entities = entity_list_init(&allocator);
    entity_t* attacker = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* defender = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){5, 5}, 10, 2, 3);

    bool result = action_try_attack(attacker, defender);
    assert_test(!result);

    entity_t *attacker_entity = attacker;
    entity_t *defender_entity = defender;
    assert_test(attacker_entity->ap == 2);
    assert_test(defender_entity->hp == 10);

    entity_list_deinit(&allocator, entities);
}

static void test_action_try_attack_fails_when_same_team(void) {
    static char buffer[1024];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_entity_t entities = entity_list_init(&allocator);
    entity_t* attacker = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* defender = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){1, 0}, 10, 2, 3);

    bool result = action_try_attack(attacker, defender);
    assert_test(!result);

    entity_t *attacker_entity = attacker;
    entity_t *defender_entity = defender;
    assert_test(attacker_entity->ap == 2);
    assert_test(defender_entity->hp == 10);

    entity_list_deinit(&allocator, entities);
}

static void test_action_try_attack_fails_when_attacker_out_of_ap(void) {
    static char buffer[1024];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_entity_t entities = entity_list_init(&allocator);
    entity_t* attacker = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* defender = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 10, 2, 3);

    attacker->ap = 0;

    bool result = action_try_attack(attacker, defender);
    assert_test(!result);

    entity_t *defender_entity = defender;
    assert_test(defender_entity->hp == 10);

    entity_list_deinit(&allocator, entities);
}

static void test_action_try_attack_fails_when_either_dead(void) {
    static char buffer[1024];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_entity_t entities = entity_list_init(&allocator);
    entity_t* attacker = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* defender = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 10, 2, 3);
    entity_t* dead_attacker = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){3, 0}, 10, 2, 3);

    entity_damage(defender, 10);
    assert_test(!defender->alive);

    bool result = action_try_attack(attacker, defender);
    assert_test(!result);
    assert_test(attacker->ap == 2);

    entity_damage(dead_attacker, 10);
    assert_test(!dead_attacker->alive);

    entity_t* other_defender = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){4, 0}, 10, 2, 3);
    result = action_try_attack(dead_attacker, other_defender);
    assert_test(!result);
    assert_test(other_defender->hp == 10);

    entity_list_deinit(&allocator, entities);
}

static void test_ai_adjacent_enemy_attacks_without_moving(void) {
    static char buffer[4096];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    grid_t grid = grid_init(&allocator, 4, 4);

    slice_entity_t entities;
    slice_t entity_align = LINEAR_ALLOCATOR_PUSH_ALIGNMENT(&allocator, entities);
    entities = entity_list_init(&allocator);
    entity_t* player = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* enemy = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 10, 2, 3);

    ai_run_enemy_phase(&allocator, grid, entities);

    entity_t *enemy_entity = enemy;
    entity_t *player_entity = player;

    assert_test(enemy_entity->position.x == 1 && enemy_entity->position.y == 0);
    assert_test(enemy_entity->ap == 1);
    assert_test(player_entity->hp == 5);
    assert_test(player_entity->alive);

    entity_list_deinit(&allocator, entities);
    linear_allocator_pop(&allocator, entity_align);
    grid_deinit(&allocator, grid);
}

static void test_ai_far_enemy_with_enough_mp_closes_and_attacks(void) {
    static char buffer[4096];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    grid_t grid = grid_init(&allocator, 5, 1);

    slice_entity_t entities;
    slice_t entity_align = LINEAR_ALLOCATOR_PUSH_ALIGNMENT(&allocator, entities);
    entities = entity_list_init(&allocator);
    entity_t* player = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* enemy = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){4, 0}, 10, 2, 4);

    ai_run_enemy_phase(&allocator, grid, entities);

    entity_t *enemy_entity = enemy;
    entity_t *player_entity = player;

    assert_test(enemy_entity->position.x == 1 && enemy_entity->position.y == 0);
    assert_test(enemy_entity->mp == 1);
    assert_test(enemy_entity->ap == 1);
    assert_test(player_entity->hp == 5);
    assert_test(player_entity->alive);

    entity_list_deinit(&allocator, entities);
    linear_allocator_pop(&allocator, entity_align);
    grid_deinit(&allocator, grid);
}

static void test_ai_insufficient_mp_moves_partial_no_attack(void) {
    static char buffer[4096];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    grid_t grid = grid_init(&allocator, 5, 1);

    slice_entity_t entities;
    slice_t entity_align = LINEAR_ALLOCATOR_PUSH_ALIGNMENT(&allocator, entities);
    entities = entity_list_init(&allocator);
    entity_t* player = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* enemy = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){4, 0}, 10, 2, 1);

    ai_run_enemy_phase(&allocator, grid, entities);

    entity_t *enemy_entity = enemy;
    entity_t *player_entity = player;

    assert_test(enemy_entity->position.x == 3 && enemy_entity->position.y == 0);
    assert_test(enemy_entity->mp == 0);
    assert_test(enemy_entity->ap == 2);
    assert_test(!entity_is_adjacent(*enemy_entity, *player_entity));
    assert_test(player_entity->hp == 10);

    entity_list_deinit(&allocator, entities);
    linear_allocator_pop(&allocator, entity_align);
    grid_deinit(&allocator, grid);
}

static void test_ai_obstacle_forces_detour(void) {
    static char buffer[4096];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    grid_t grid = grid_init(&allocator, 5, 3);
    grid_set_walkable(grid, (position_t){2, 0}, false);
    grid_set_walkable(grid, (position_t){2, 1}, false);

    slice_entity_t entities;
    slice_t entity_align = LINEAR_ALLOCATOR_PUSH_ALIGNMENT(&allocator, entities);
    entities = entity_list_init(&allocator);
    entity_t* player = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){4, 1}, 10, 2, 3);
    entity_t* enemy = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){0, 1}, 10, 2, 8);

    ai_run_enemy_phase(&allocator, grid, entities);

    entity_t *enemy_entity = enemy;
    entity_t *player_entity = player;

    assert_test(entity_is_adjacent(*enemy_entity, *player_entity));
    assert_test(player_entity->hp == 5);
    assert_test(enemy_entity->ap == 1);

    entity_list_deinit(&allocator, entities);
    linear_allocator_pop(&allocator, entity_align);
    grid_deinit(&allocator, grid);
}

static void test_ai_multiple_enemies_act_independently_in_ascending_id_order(void) {
    static char buffer[4096];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    grid_t grid = grid_init(&allocator, 4, 4);

    slice_entity_t entities;
    slice_t entity_align = LINEAR_ALLOCATOR_PUSH_ALIGNMENT(&allocator, entities);
    entities = entity_list_init(&allocator);
    entity_t* player = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 30, 2, 3);
    entity_t* enemy_a = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 0}, 10, 2, 3);
    entity_t* enemy_b = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){0, 3}, 10, 2, 3);

    assert_test(enemy_a < enemy_b);

    ai_run_enemy_phase(&allocator, grid, entities);

    entity_t *player_entity = player;
    entity_t *a_entity = enemy_a;
    entity_t *b_entity = enemy_b;

    assert_test(a_entity->position.x == 1 && a_entity->position.y == 0);
    assert_test(a_entity->mp == 1);
    assert_test(a_entity->ap == 1);

    assert_test(b_entity->position.x == 0 && b_entity->position.y == 1);
    assert_test(b_entity->mp == 1);
    assert_test(b_entity->ap == 1);

    assert_test(player_entity->hp == 20);
    assert_test(player_entity->alive);

    entity_list_deinit(&allocator, entities);
    linear_allocator_pop(&allocator, entity_align);
    grid_deinit(&allocator, grid);
}

static void test_ai_zero_mp_not_adjacent_does_nothing(void) {
    static char buffer[4096];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    grid_t grid = grid_init(&allocator, 4, 4);

    slice_entity_t entities;
    slice_t entity_align = LINEAR_ALLOCATOR_PUSH_ALIGNMENT(&allocator, entities);
    entities = entity_list_init(&allocator);
    entity_t* player = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* enemy = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 3}, 10, 2, 0);

    ai_run_enemy_phase(&allocator, grid, entities);

    entity_t *enemy_entity = enemy;
    entity_t *player_entity = player;

    assert_test(enemy_entity->position.x == 3 && enemy_entity->position.y == 3);
    assert_test(enemy_entity->mp == 0);
    assert_test(enemy_entity->ap == 2);
    assert_test(player_entity->hp == 10);

    entity_list_deinit(&allocator, entities);
    linear_allocator_pop(&allocator, entity_align);
    grid_deinit(&allocator, grid);
}

// Shared layout used by game orchestration tests: grid_width=16,
// grid_height=10, fb 320x240, hud_height=40 -- same numbers as
// test_layout_compute_defaults, so tile_size=20 and end_turn_button is the
// known rect x=250,y=210,w=60,h=20.
#define GAME_TEST_GRID_WIDTH 16
#define GAME_TEST_GRID_HEIGHT 10
#define GAME_TEST_FB_WIDTH 320
#define GAME_TEST_FB_HEIGHT 240
#define GAME_TEST_HUD_HEIGHT 40

static void test_game_entity_pressed_selects_own_and_switches_selection(void) {
    static char buffer[8192];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    grid_t grid = grid_init(&allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_entity_t entities = entity_list_init(&allocator);
    game_state_t game = game_init(grid, entities, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    entity_t* p1 = entity_spawn(&allocator, &game.entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* p2 = entity_spawn(&allocator, &game.entities, ENTITY_TEAM_PLAYER, (position_t){1, 0}, 10, 2, 3);

    game_on_entity_pressed(&game, p1);
    assert_test(game.selected_entity == p1);

    game_on_entity_pressed(&game, p2);
    assert_test(game.selected_entity == p2);

    game_deinit(&allocator, game);
}

static void test_game_entity_pressed_enemy_with_none_selected_noops(void) {
    static char buffer[8192];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    grid_t grid = grid_init(&allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_entity_t entities = entity_list_init(&allocator);
    game_state_t game = game_init(grid, entities, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    entity_t* e1 = entity_spawn(&allocator, &game.entities, ENTITY_TEAM_ENEMY, (position_t){5, 5}, 10, 2, 3);

    game_on_entity_pressed(&game, e1);
    assert_test(game.selected_entity == 0);

    game_deinit(&allocator, game);
}

static void test_game_entity_pressed_adjacent_enemy_attacks_then_noops_when_ap_zero(void) {
    static char buffer[8192];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    grid_t grid = grid_init(&allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_entity_t entities = entity_list_init(&allocator);
    game_state_t game = game_init(grid, entities, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    entity_t* p = entity_spawn(&allocator, &game.entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 1, 3);
    entity_t* e = entity_spawn(&allocator, &game.entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 10, 2, 3);

    game_on_entity_pressed(&game, p);
    assert_test(game.selected_entity == p);

    game_on_entity_pressed(&game, e);
    assert_test(p->ap == 0);
    assert_test(e->hp == 5);
    assert_test(e->alive);

    game_on_entity_pressed(&game, e);
    assert_test(p->ap == 0);
    assert_test(e->hp == 5);

    game_deinit(&allocator, game);
}

static void test_game_tile_pressed_moves_within_reach_and_consumes_mp(void) {
    static char buffer[8192];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    grid_t grid = grid_init(&allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_entity_t entities = entity_list_init(&allocator);
    game_state_t game = game_init(grid, entities, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    entity_t* p = entity_spawn(&allocator, &game.entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);

    game_on_entity_pressed(&game, p);
    game_on_tile_pressed(&game, &allocator, (position_t){2, 0});

    entity_t *entity = p;
    assert_test(entity->position.x == 2 && entity->position.y == 0);
    assert_test(entity->mp == 1);

    game_deinit(&allocator, game);
}

static void test_game_tile_pressed_noops_on_unreachable_tile(void) {
    static char buffer[8192];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    grid_t grid = grid_init(&allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_entity_t entities = entity_list_init(&allocator);
    game_state_t game = game_init(grid, entities, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    entity_t* p = entity_spawn(&allocator, &game.entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 1);

    game_on_entity_pressed(&game, p);
    game_on_tile_pressed(&game, &allocator, (position_t){5, 0});

    entity_t *entity = p;
    assert_test(entity->position.x == 0 && entity->position.y == 0);
    assert_test(entity->mp == 1);

    game_deinit(&allocator, game);
}

static void test_game_end_turn_resets_player_phase_and_deselects(void) {
    static char buffer[8192];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    grid_t grid = grid_init(&allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_entity_t entities = entity_list_init(&allocator);
    game_state_t game = game_init(grid, entities, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    entity_t* p = entity_spawn(&allocator, &game.entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_spawn(&allocator, &game.entities, ENTITY_TEAM_ENEMY, (position_t){15, 9}, 10, 2, 0); // far away, zero mp: can't reach or attack

    p->ap = 0;
    p->mp = 0;

    game_on_entity_pressed(&game, p);
    assert_test(game.selected_entity == p);

    game_on_end_turn_pressed(&game, &allocator);

    assert_test(game.turn.phase == TURN_PHASE_PLAYER);
    assert_test(game.turn.turn_number == 2);
    assert_test(game.selected_entity == 0);

    entity_t *player = p;
    assert_test(player->ap == player->max_ap);
    assert_test(player->mp == player->max_mp);
    assert_test(player->alive);

    game_deinit(&allocator, game);
}

static void test_game_1v1_enemy_death_sets_win_and_freezes_input(void) {
    static char buffer[8192];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    grid_t grid = grid_init(&allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_entity_t entities = entity_list_init(&allocator);
    game_state_t game = game_init(grid, entities, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    entity_t* p = entity_spawn(&allocator, &game.entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* e = entity_spawn(&allocator, &game.entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 5, 2, 3);

    game_on_entity_pressed(&game, p);
    game_on_entity_pressed(&game, e);

    assert_test(!e->alive);
    assert_test(game.game_over == GAME_OVER_WIN);

    // Further presses of any kind must now be frozen no-ops.
    game_on_tile_pressed(&game, &allocator, (position_t){2, 0});
    entity_t *player = p;
    assert_test(player->position.x == 0 && player->position.y == 0);

    turn_phase_t phase_before = game.turn.phase;
    int turn_number_before = game.turn.turn_number;
    game_on_end_turn_pressed(&game, &allocator);
    assert_test(game.turn.phase == phase_before);
    assert_test(game.turn.turn_number == turn_number_before);

    assert_test(game.game_over == GAME_OVER_WIN);

    game_deinit(&allocator, game);
}

static void test_game_ai_kills_last_player_during_end_turn_sets_lose(void) {
    static char buffer[8192];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    grid_t grid = grid_init(&allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_entity_t entities = entity_list_init(&allocator);
    game_state_t game = game_init(grid, entities, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    entity_t* p = entity_spawn(&allocator, &game.entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 5, 2, 3);
    entity_spawn(&allocator, &game.entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 10, 2, 3);

    assert_test(game.turn.phase == TURN_PHASE_PLAYER);

    game_on_end_turn_pressed(&game, &allocator);

    assert_test(!p->alive);
    assert_test(game.game_over == GAME_OVER_LOSE);

    // Further presses must be frozen no-ops.
    turn_phase_t phase_before = game.turn.phase;
    game_on_end_turn_pressed(&game, &allocator);
    assert_test(game.turn.phase == phase_before);

    game_deinit(&allocator, game);
}

static void test_game_on_input_event_click_in_end_turn_button_behaves_like_end_turn_pressed(void) {
    static char buffer[8192];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    grid_t grid = grid_init(&allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_entity_t entities = entity_list_init(&allocator);
    game_state_t game = game_init(grid, entities, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    entity_t* p = entity_spawn(&allocator, &game.entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_spawn(&allocator, &game.entities, ENTITY_TEAM_ENEMY, (position_t){15, 9}, 10, 2, 0); // far away, zero mp: can't reach or attack
    p->ap = 0;
    p->mp = 0;

    game_on_entity_pressed(&game, p);
    assert_test(game.selected_entity == p);

    assert_test(point_in_rect(game.viewport.end_turn_button, 260, 215));
    input_event_t click = { .type = INPUT_EVENT_MOUSE_CLICK, .x = 260, .y = 215 };
    game_on_input_event(&game, &allocator, click);

    assert_test(game.turn.phase == TURN_PHASE_PLAYER);
    assert_test(game.turn.turn_number == 2);
    assert_test(game.selected_entity == 0);

    entity_t *player = p;
    assert_test(player->ap == player->max_ap);
    assert_test(player->mp == player->max_mp);

    game_deinit(&allocator, game);
}

static void test_game_on_input_event_click_on_entity_tile_behaves_like_entity_pressed(void) {
    static char buffer[8192];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    grid_t grid = grid_init(&allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_entity_t entities = entity_list_init(&allocator);
    game_state_t game = game_init(grid, entities, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    entity_spawn(&allocator, &game.entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* p2 = entity_spawn(&allocator, &game.entities, ENTITY_TEAM_PLAYER, (position_t){3, 3}, 10, 2, 3);

    int px, py;
    grid_to_screen(game.viewport, 3, 3, &px, &py);

    input_event_t click = { .type = INPUT_EVENT_MOUSE_CLICK, .x = px + 1, .y = py + 1 };
    game_on_input_event(&game, &allocator, click);

    assert_test(game.selected_entity == p2);

    game_deinit(&allocator, game);
}

static void test_scenario_setup_default_populates_map_and_units(void) {
    static char buffer[8192];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    game_state_t game = scenario_setup_default(&allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    assert_test(SLICE_TYPESIZE(game.entities) == 6);

    struct {
        int x, y;
        entity_team_t team;
    } expected[6] = {
        { 1, 2, ENTITY_TEAM_PLAYER },
        { 1, 5, ENTITY_TEAM_PLAYER },
        { 1, 8, ENTITY_TEAM_PLAYER },
        { 14, 2, ENTITY_TEAM_ENEMY },
        { 14, 5, ENTITY_TEAM_ENEMY },
        { 14, 8, ENTITY_TEAM_ENEMY },
    };

    for (int id = 0; id < 6; id++) {
        entity_t *entity = &SLICE_AT(game.entities, id);
        assert_test(entity->position.x == expected[id].x);
        assert_test(entity->position.y == expected[id].y);
        assert_test(entity->team == expected[id].team);
        assert_test(entity->hp == 10 && entity->max_hp == 10);
        assert_test(entity->ap == 1 && entity->max_ap == 1);
        assert_test(entity->mp == 3 && entity->max_mp == 3);
        assert_test(entity->alive);
    }

    assert_test(!grid_is_walkable(game.grid, (position_t){7, 4}));
    assert_test(!grid_is_walkable(game.grid, (position_t){7, 5}));

    for (int y = 0; y < game.grid.height; y++) {
        for (int x = 0; x < game.grid.width; x++) {
            if ((x == 7 && y == 4) || (x == 7 && y == 5)) {
                continue;
            }
            assert_test(grid_is_walkable(game.grid, (position_t){x, y}));
        }
    }

    assert_test(entity_alive_count(game.entities, ENTITY_TEAM_PLAYER) == 3);
    assert_test(entity_alive_count(game.entities, ENTITY_TEAM_ENEMY) == 3);

    game_deinit(&allocator, game);
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
    { TEST_NAME("entity_find_at_ignores_dead_and_empty"), test_entity_find_at_ignores_dead_and_empty },
    { TEST_NAME("entity_damage_clamps_and_flips_alive"), test_entity_damage_clamps_and_flips_alive },
    { TEST_NAME("entity_is_adjacent"), test_entity_is_adjacent },
    { TEST_NAME("entity_alive_count_per_team"), test_entity_alive_count_per_team },
    { TEST_NAME("layout_compute_defaults"), test_layout_compute_defaults },
    { TEST_NAME("point_in_rect"), test_point_in_rect },
    { TEST_NAME("screen_to_grid_corners"), test_screen_to_grid_corners },
    { TEST_NAME("grid_to_screen_round_trips_with_screen_to_grid"), test_grid_to_screen_round_trips_with_screen_to_grid },
    { TEST_NAME("pathing_flat_grid_distance"), test_pathing_flat_grid_distance },
    { TEST_NAME("pathing_obstacle_forces_detour"), test_pathing_obstacle_forces_detour },
    { TEST_NAME("pathing_occupied_tile_blocks_corridor"), test_pathing_occupied_tile_blocks_corridor },
    { TEST_NAME("pathing_skip_entity_allows_own_start_tile"), test_pathing_skip_entity_allows_own_start_tile },
    { TEST_NAME("pathing_max_steps_caps_distance"), test_pathing_max_steps_caps_distance },
    { TEST_NAME("turn_init_starts_player_turn_one"), test_turn_init_starts_player_turn_one },
    { TEST_NAME("turn_reset_team_points_ignores_dead_entities"), test_turn_reset_team_points_ignores_dead_entities },
    { TEST_NAME("turn_reset_team_points_only_affects_matching_team"), test_turn_reset_team_points_only_affects_matching_team },
    { TEST_NAME("turn_begin_enemy_phase_resets_only_enemy_team"), test_turn_begin_enemy_phase_resets_only_enemy_team },
    { TEST_NAME("turn_begin_player_phase_resets_only_player_team_and_increments_turn"), test_turn_begin_player_phase_resets_only_player_team_and_increments_turn },
    { TEST_NAME("action_try_move_succeeds_within_mp"), test_action_try_move_succeeds_within_mp },
    { TEST_NAME("action_try_move_fails_beyond_mp"), test_action_try_move_fails_beyond_mp },
    { TEST_NAME("action_try_move_fails_onto_obstacle"), test_action_try_move_fails_onto_obstacle },
    { TEST_NAME("action_try_move_fails_onto_occupied_tile"), test_action_try_move_fails_onto_occupied_tile },
    { TEST_NAME("action_try_attack_succeeds_on_adjacent_enemy"), test_action_try_attack_succeeds_on_adjacent_enemy },
    { TEST_NAME("action_try_attack_killing_blow_leaves_defender_dead"), test_action_try_attack_killing_blow_leaves_defender_dead },
    { TEST_NAME("action_try_attack_fails_when_not_adjacent"), test_action_try_attack_fails_when_not_adjacent },
    { TEST_NAME("action_try_attack_fails_when_same_team"), test_action_try_attack_fails_when_same_team },
    { TEST_NAME("action_try_attack_fails_when_attacker_out_of_ap"), test_action_try_attack_fails_when_attacker_out_of_ap },
    { TEST_NAME("action_try_attack_fails_when_either_dead"), test_action_try_attack_fails_when_either_dead },
    { TEST_NAME("ai_adjacent_enemy_attacks_without_moving"), test_ai_adjacent_enemy_attacks_without_moving },
    { TEST_NAME("ai_far_enemy_with_enough_mp_closes_and_attacks"), test_ai_far_enemy_with_enough_mp_closes_and_attacks },
    { TEST_NAME("ai_insufficient_mp_moves_partial_no_attack"), test_ai_insufficient_mp_moves_partial_no_attack },
    { TEST_NAME("ai_obstacle_forces_detour"), test_ai_obstacle_forces_detour },
    { TEST_NAME("ai_multiple_enemies_act_independently_in_ascending_id_order"), test_ai_multiple_enemies_act_independently_in_ascending_id_order },
    { TEST_NAME("ai_zero_mp_not_adjacent_does_nothing"), test_ai_zero_mp_not_adjacent_does_nothing },
    { TEST_NAME("game_entity_pressed_selects_own_and_switches_selection"), test_game_entity_pressed_selects_own_and_switches_selection },
    { TEST_NAME("game_entity_pressed_enemy_with_none_selected_noops"), test_game_entity_pressed_enemy_with_none_selected_noops },
    { TEST_NAME("game_entity_pressed_adjacent_enemy_attacks_then_noops_when_ap_zero"), test_game_entity_pressed_adjacent_enemy_attacks_then_noops_when_ap_zero },
    { TEST_NAME("game_tile_pressed_moves_within_reach_and_consumes_mp"), test_game_tile_pressed_moves_within_reach_and_consumes_mp },
    { TEST_NAME("game_tile_pressed_noops_on_unreachable_tile"), test_game_tile_pressed_noops_on_unreachable_tile },
    { TEST_NAME("game_end_turn_resets_player_phase_and_deselects"), test_game_end_turn_resets_player_phase_and_deselects },
    { TEST_NAME("game_1v1_enemy_death_sets_win_and_freezes_input"), test_game_1v1_enemy_death_sets_win_and_freezes_input },
    { TEST_NAME("game_ai_kills_last_player_during_end_turn_sets_lose"), test_game_ai_kills_last_player_during_end_turn_sets_lose },
    { TEST_NAME("game_on_input_event_click_in_end_turn_button_behaves_like_end_turn_pressed"), test_game_on_input_event_click_in_end_turn_button_behaves_like_end_turn_pressed },
    { TEST_NAME("game_on_input_event_click_on_entity_tile_behaves_like_entity_pressed"), test_game_on_input_event_click_on_entity_tile_behaves_like_entity_pressed },
    { TEST_NAME("scenario_setup_default_populates_map_and_units"), test_scenario_setup_default_populates_map_and_units },
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
