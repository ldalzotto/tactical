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

// game_on_entity_pressed/game_on_tile_pressed/game_on_end_turn_pressed are
// private to game.c: drive them the same way real input does, through
// game_on_input_event.
static void test_click_tile(game_state_t *game, linear_allocator_t *allocator, position_t target) {
    int px, py;
    grid_to_screen(game->viewport, target.x, target.y, &px, &py);
    input_event_t click = { .type = INPUT_EVENT_MOUSE_CLICK, .x = px + 1, .y = py + 1 };
    game_on_input_event(game, allocator, click);
}

static void test_click_end_turn(game_state_t *game, linear_allocator_t *allocator) {
    rect_t button = game->viewport.end_turn_button;
    input_event_t click = { .type = INPUT_EVENT_MOUSE_CLICK, .x = button.x + 1, .y = button.y + 1 };
    game_on_input_event(game, allocator, click);
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

// The tests below drive game subsystems (grid, entity, pathing, turn,
// action, ai) only through game_init's public surface -- game_on_input_event
// via test_click_tile/test_click_end_turn -- and assert on game_state_t
// fields, the same way a player and a screen reader of the board would.
// Behavior that the game API structurally prevents a player from ever
// triggering (attacking a target the UI won't let you select, moving onto a
// tile the UI routes to a different handler) has no equivalent test here:
// there's no click that reaches it.

static void test_game_selecting_entity_computes_reachable_tiles_within_mp_and_moves(void) {
    static char buffer[4096];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_t grid_padding = grid_align(&allocator);
    grid_t grid = grid_init(&allocator, 4, 4);
    slice_t entity_list_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(&allocator);
    entity_t* p = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 2);

    slice_t turn_order_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(&allocator);
    turn_order_add(&allocator, &order, p);

    game_state_t game = game_init(&allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, &allocator, p->position);
    assert_test(game.selected_entity == p);

    // mp is 2: (0,0) is dist 0 (excluded, it's where the mover stands),
    // (2,0) is dist 2 (in range), (3,0) is dist 3 and (3,3) is dist 6 (both
    // beyond mp) -- so the highlighted set doubles as a check that BFS
    // distance and the max-steps cap both land where expected.
    bool own_tile_reachable = false, near_tile_reachable = false, far_tile_reachable = false, corner_tile_reachable = false;
    for (int i = 0; i < (int)SLICE_TYPESIZE(game.render.reachable_tiles); i++) {
        position_t tile = SLICE_AT(game.render.reachable_tiles, i);
        if (tile.x == 0 && tile.y == 0) own_tile_reachable = true;
        if (tile.x == 2 && tile.y == 0) near_tile_reachable = true;
        if (tile.x == 3 && tile.y == 0) far_tile_reachable = true;
        if (tile.x == 3 && tile.y == 3) corner_tile_reachable = true;
    }
    assert_test(!own_tile_reachable);
    assert_test(near_tile_reachable);
    assert_test(!far_tile_reachable);
    assert_test(!corner_tile_reachable);

    test_click_tile(&game, &allocator, (position_t){2, 0});

    entity_t *entity = p;
    assert_test(entity->position.x == 2 && entity->position.y == 0);
    assert_test(entity->mp == 0);

    game_deinit(&allocator, game);
}

static void test_game_obstacles_block_reachable_tiles_and_movement(void) {
    static char buffer[4096];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_t grid_padding = grid_align(&allocator);
    grid_t grid = grid_init(&allocator, 5, 3);
    grid_set_walkable(grid, (position_t){2, 0}, false);
    grid_set_walkable(grid, (position_t){2, 1}, false);

    slice_t entity_list_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(&allocator);
    entity_t* p = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 1}, 10, 2, 4);

    slice_t turn_order_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(&allocator);
    turn_order_add(&allocator, &order, p);

    game_state_t game = game_init(&allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, &allocator, p->position);

    bool obstacle_a_reachable = false, obstacle_b_reachable = false, near_side_reachable = false, far_side_reachable = false;
    for (int i = 0; i < (int)SLICE_TYPESIZE(game.render.reachable_tiles); i++) {
        position_t tile = SLICE_AT(game.render.reachable_tiles, i);
        if (tile.x == 2 && tile.y == 0) obstacle_a_reachable = true;
        if (tile.x == 2 && tile.y == 1) obstacle_b_reachable = true;
        if (tile.x == 1 && tile.y == 1) near_side_reachable = true;
        // (4,1) sits right past the wall: with mp 4, it's only in range if
        // the walk-around-the-wall path (6 tiles) is what BFS actually took
        // -- the direct 4-tile path is blocked, so it must be absent.
        if (tile.x == 4 && tile.y == 1) far_side_reachable = true;
    }
    assert_test(!obstacle_a_reachable);
    assert_test(!obstacle_b_reachable);
    assert_test(near_side_reachable);
    assert_test(!far_side_reachable);

    // Clicking straight onto the wall is a no-op: unwalkable tiles never
    // become a valid move target, wall or no wall around it.
    test_click_tile(&game, &allocator, (position_t){2, 0});

    entity_t *entity = p;
    assert_test(entity->position.x == 0 && entity->position.y == 1);
    assert_test(entity->mp == 4);

    game_deinit(&allocator, game);
}

static void test_game_occupied_tile_blocks_corridor_reachability(void) {
    static char buffer[4096];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_t grid_padding = grid_align(&allocator);
    grid_t grid = grid_init(&allocator, 5, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(&allocator);
    entity_t* p = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 10);
    entity_t* blocker = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){2, 0}, 10, 2, 3);

    slice_t turn_order_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(&allocator);
    turn_order_add(&allocator, &order, p);
    turn_order_add(&allocator, &order, blocker);

    game_state_t game = game_init(&allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, &allocator, p->position);

    bool before_blocker_reachable = false, blocker_tile_reachable = false, past_blocker_reachable = false, corridor_end_reachable = false;
    for (int i = 0; i < (int)SLICE_TYPESIZE(game.render.reachable_tiles); i++) {
        position_t tile = SLICE_AT(game.render.reachable_tiles, i);
        if (tile.x == 1 && tile.y == 0) before_blocker_reachable = true;
        if (tile.x == 2 && tile.y == 0) blocker_tile_reachable = true;
        if (tile.x == 3 && tile.y == 0) past_blocker_reachable = true;
        if (tile.x == 4 && tile.y == 0) corridor_end_reachable = true;
    }
    assert_test(before_blocker_reachable);
    // The occupied tile itself, and everything past it in this single-file
    // corridor, are unreachable: the living blocker seals the corridor even
    // though the mover has plenty of mp to cross it.
    assert_test(!blocker_tile_reachable);
    assert_test(!past_blocker_reachable);
    assert_test(!corridor_end_reachable);

    game_deinit(&allocator, game);
}

static void test_game_attack_kills_defender_clamps_hp_and_frees_tile_for_movement(void) {
    static char buffer[4096];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_t grid_padding = grid_align(&allocator);
    grid_t grid = grid_init(&allocator, 4, 4);
    slice_t entity_list_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(&allocator);
    entity_t* p = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 5);
    entity_t* e = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 3, 2, 3);
    entity_t* e2 = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 3}, 10, 2, 3);

    slice_t turn_order_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(&allocator);
    turn_order_add(&allocator, &order, p);
    turn_order_add(&allocator, &order, e);
    turn_order_add(&allocator, &order, e2);

    game_state_t game = game_init(&allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, &allocator, p->position);
    test_click_tile(&game, &allocator, e->position);

    // Attack damage (5) exceeds e's hp (3): hp clamps to 0, not negative.
    assert_test(e->hp == 0);
    assert_test(!e->alive);
    assert_test(p->ap == 1);
    // e2 is still alive, so the enemy team isn't wiped out yet.
    assert_test(game.game_over == GAME_OVER_NONE);

    // e's corpse no longer occupies its tile: pressing it now falls through
    // to a move, same as any other empty tile.
    test_click_tile(&game, &allocator, (position_t){1, 0});

    entity_t *entity = p;
    assert_test(entity->position.x == 1 && entity->position.y == 0);
    assert_test(entity->mp == 4);

    game_deinit(&allocator, game);
}

static void test_game_entity_pressed_diagonal_and_far_enemy_attack_noop(void) {
    static char buffer[4096];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_t grid_padding = grid_align(&allocator);
    grid_t grid = grid_init(&allocator, 6, 6);
    slice_t entity_list_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(&allocator);
    entity_t* p = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* diagonal = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 1}, 10, 2, 3);
    entity_t* far = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){5, 5}, 10, 2, 3);

    slice_t turn_order_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(&allocator);
    turn_order_add(&allocator, &order, p);
    turn_order_add(&allocator, &order, diagonal);
    turn_order_add(&allocator, &order, far);

    game_state_t game = game_init(&allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, &allocator, p->position);
    assert_test(game.selected_entity == p);

    // Diagonal doesn't count as adjacent: pressing it is a no-op.
    test_click_tile(&game, &allocator, diagonal->position);
    assert_test(p->ap == 2);
    assert_test(diagonal->hp == 10);

    // Neither does simply being far away.
    test_click_tile(&game, &allocator, far->position);
    assert_test(p->ap == 2);
    assert_test(far->hp == 10);

    game_deinit(&allocator, game);
}

static void test_game_turn_order_compacts_when_non_active_entity_dies_during_attack(void) {
    static char buffer[4096];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_t grid_padding = grid_align(&allocator);
    grid_t grid = grid_init(&allocator, 4, 4);
    slice_t entity_list_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(&allocator);
    entity_t* p = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* b = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 3}, 10, 2, 3);
    entity_t* c = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 3, 2, 3);

    slice_t turn_order_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(&allocator);
    turn_order_add(&allocator, &order, p);
    turn_order_add(&allocator, &order, b);
    turn_order_add(&allocator, &order, c);

    game_state_t game = game_init(&allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, &allocator, p->position);
    test_click_tile(&game, &allocator, c->position);

    assert_test(!c->alive);
    assert_test(SLICE_TYPESIZE(game.turn.order) == 2);
    assert_test(SLICE_AT(game.turn.order, 0) == p);
    assert_test(SLICE_AT(game.turn.order, 1) == b);
    assert_test(turn_active_entity(game.turn) == p);

    game_deinit(&allocator, game);
}

static void test_game_ai_adjacent_enemy_attacks_without_moving_on_end_turn(void) {
    static char buffer[4096];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_t grid_padding = grid_align(&allocator);
    grid_t grid = grid_init(&allocator, 4, 4);
    slice_t entity_list_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(&allocator);
    entity_t* p = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* enemy = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 10, 2, 3);

    slice_t turn_order_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(&allocator);
    turn_order_add(&allocator, &order, p);
    turn_order_add(&allocator, &order, enemy);

    game_state_t game = game_init(&allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, 320, 240, 40);

    test_click_end_turn(&game, &allocator);

    assert_test(turn_active_entity(game.turn) == p);
    assert_test(enemy->position.x == 1 && enemy->position.y == 0);
    assert_test(enemy->ap == 1);
    assert_test(p->hp == 5);
    assert_test(p->alive);

    game_deinit(&allocator, game);
}

static void test_game_ai_far_enemy_with_enough_mp_closes_and_attacks_on_end_turn(void) {
    static char buffer[4096];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_t grid_padding = grid_align(&allocator);
    grid_t grid = grid_init(&allocator, 5, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(&allocator);
    entity_t* p = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* enemy = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){4, 0}, 10, 2, 4);

    slice_t turn_order_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(&allocator);
    turn_order_add(&allocator, &order, p);
    turn_order_add(&allocator, &order, enemy);

    game_state_t game = game_init(&allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, 320, 240, 40);

    test_click_end_turn(&game, &allocator);

    assert_test(turn_active_entity(game.turn) == p);
    assert_test(enemy->position.x == 1 && enemy->position.y == 0);
    assert_test(enemy->mp == 1);
    assert_test(enemy->ap == 1);
    assert_test(p->hp == 5);
    assert_test(p->alive);

    game_deinit(&allocator, game);
}

static void test_game_ai_insufficient_mp_moves_partial_no_attack_on_end_turn(void) {
    static char buffer[4096];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_t grid_padding = grid_align(&allocator);
    grid_t grid = grid_init(&allocator, 5, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(&allocator);
    entity_t* p = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* enemy = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){4, 0}, 10, 2, 1);

    slice_t turn_order_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(&allocator);
    turn_order_add(&allocator, &order, p);
    turn_order_add(&allocator, &order, enemy);

    game_state_t game = game_init(&allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, 320, 240, 40);

    test_click_end_turn(&game, &allocator);

    assert_test(turn_active_entity(game.turn) == p);
    assert_test(enemy->position.x == 3 && enemy->position.y == 0);
    assert_test(enemy->mp == 0);
    assert_test(enemy->ap == 2);
    assert_test(!entity_is_adjacent(*enemy, *p));
    assert_test(p->hp == 10);

    game_deinit(&allocator, game);
}

static void test_game_ai_obstacle_forces_detour_on_end_turn(void) {
    static char buffer[4096];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_t grid_padding = grid_align(&allocator);
    grid_t grid = grid_init(&allocator, 5, 3);
    grid_set_walkable(grid, (position_t){2, 0}, false);
    grid_set_walkable(grid, (position_t){2, 1}, false);

    slice_t entity_list_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(&allocator);
    entity_t* p = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){4, 1}, 10, 2, 3);
    entity_t* enemy = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){0, 1}, 10, 2, 8);

    slice_t turn_order_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(&allocator);
    turn_order_add(&allocator, &order, p);
    turn_order_add(&allocator, &order, enemy);

    game_state_t game = game_init(&allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, 320, 240, 40);

    test_click_end_turn(&game, &allocator);

    assert_test(turn_active_entity(game.turn) == p);
    assert_test(entity_is_adjacent(*enemy, *p));
    assert_test(p->hp == 5);
    assert_test(enemy->ap == 1);

    game_deinit(&allocator, game);
}

static void test_game_ai_multiple_enemies_act_independently_on_end_turn(void) {
    static char buffer[4096];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_t grid_padding = grid_align(&allocator);
    grid_t grid = grid_init(&allocator, 4, 4);
    slice_t entity_list_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(&allocator);
    entity_t* p = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 30, 2, 3);
    entity_t* enemy_a = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 0}, 10, 2, 3);
    entity_t* enemy_b = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){0, 3}, 10, 2, 3);

    slice_t turn_order_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(&allocator);
    turn_order_add(&allocator, &order, p);
    turn_order_add(&allocator, &order, enemy_a);
    turn_order_add(&allocator, &order, enemy_b);

    game_state_t game = game_init(&allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, 320, 240, 40);

    // One end-turn press plays out both enemies' turns in order, back
    // around to the player.
    test_click_end_turn(&game, &allocator);

    assert_test(turn_active_entity(game.turn) == p);

    assert_test(enemy_a->position.x == 1 && enemy_a->position.y == 0);
    assert_test(enemy_a->mp == 1);
    assert_test(enemy_a->ap == 1);

    assert_test(enemy_b->position.x == 0 && enemy_b->position.y == 1);
    assert_test(enemy_b->mp == 1);
    assert_test(enemy_b->ap == 1);

    assert_test(p->hp == 20);
    assert_test(p->alive);

    game_deinit(&allocator, game);
}

static void test_game_ai_zero_mp_not_adjacent_does_nothing_on_end_turn(void) {
    static char buffer[4096];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_t grid_padding = grid_align(&allocator);
    grid_t grid = grid_init(&allocator, 4, 4);
    slice_t entity_list_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(&allocator);
    entity_t* p = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* enemy = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 3}, 10, 2, 0);

    slice_t turn_order_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(&allocator);
    turn_order_add(&allocator, &order, p);
    turn_order_add(&allocator, &order, enemy);

    game_state_t game = game_init(&allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, 320, 240, 40);

    test_click_end_turn(&game, &allocator);

    assert_test(turn_active_entity(game.turn) == p);
    assert_test(enemy->position.x == 3 && enemy->position.y == 3);
    assert_test(enemy->mp == 0);
    assert_test(enemy->ap == 2);
    assert_test(p->hp == 10);

    game_deinit(&allocator, game);
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


static void test_game_entity_pressed_selects_only_the_active_entity(void) {
    static char buffer[8192];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_t grid_padding = grid_align(&allocator);
    grid_t grid = grid_init(&allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_t entity_list_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(&allocator);
    entity_t* p1 = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* p2 = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){1, 0}, 10, 2, 3);

    slice_t turn_order_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(&allocator);
    turn_order_add(&allocator, &order, p1);
    turn_order_add(&allocator, &order, p2);

    game_state_t game = game_init(&allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);
    assert_test(turn_active_entity(game.turn) == p1);

    test_click_tile(&game, &allocator, p1->position);
    assert_test(game.selected_entity == p1);

    // p2 isn't the active entity: pressing it is a no-op, including as an
    // attack target -- same-team damage never lands.
    test_click_tile(&game, &allocator, p2->position);
    assert_test(game.selected_entity == p1);
    assert_test(p1->ap == 2);
    assert_test(p2->hp == 10);

    game_deinit(&allocator, game);
}

static void test_game_entity_pressed_enemy_active_noops(void) {
    static char buffer[8192];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_t grid_padding = grid_align(&allocator);
    grid_t grid = grid_init(&allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_t entity_list_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(&allocator);
    entity_t* e1 = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){5, 5}, 10, 2, 3);

    slice_t turn_order_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(&allocator);
    turn_order_add(&allocator, &order, e1);

    game_state_t game = game_init(&allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    test_click_tile(&game, &allocator, e1->position);
    assert_test(game.selected_entity == 0);

    game_deinit(&allocator, game);
}

static void test_game_entity_pressed_adjacent_enemy_attacks_then_noops_when_ap_zero(void) {
    static char buffer[8192];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_t grid_padding = grid_align(&allocator);
    grid_t grid = grid_init(&allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_t entity_list_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(&allocator);
    entity_t* p = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 1, 3);
    entity_t* e = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 10, 2, 3);

    slice_t turn_order_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(&allocator);
    turn_order_add(&allocator, &order, p);
    turn_order_add(&allocator, &order, e);

    game_state_t game = game_init(&allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    test_click_tile(&game, &allocator, p->position);
    assert_test(game.selected_entity == p);

    test_click_tile(&game, &allocator, e->position);
    assert_test(p->ap == 0);
    assert_test(e->hp == 5);
    assert_test(e->alive);

    test_click_tile(&game, &allocator, e->position);
    assert_test(p->ap == 0);
    assert_test(e->hp == 5);

    game_deinit(&allocator, game);
}

static void test_game_tile_pressed_moves_within_reach_and_consumes_mp(void) {
    static char buffer[8192];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_t grid_padding = grid_align(&allocator);
    grid_t grid = grid_init(&allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_t entity_list_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(&allocator);
    entity_t* p = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);

    slice_t turn_order_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(&allocator);
    turn_order_add(&allocator, &order, p);

    game_state_t game = game_init(&allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    test_click_tile(&game, &allocator, p->position);
    test_click_tile(&game, &allocator, (position_t){2, 0});

    entity_t *entity = p;
    assert_test(entity->position.x == 2 && entity->position.y == 0);
    assert_test(entity->mp == 1);

    game_deinit(&allocator, game);
}

static void test_game_tile_pressed_noops_on_unreachable_tile(void) {
    static char buffer[8192];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_t grid_padding = grid_align(&allocator);
    grid_t grid = grid_init(&allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_t entity_list_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(&allocator);
    entity_t* p = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 1);

    slice_t turn_order_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(&allocator);
    turn_order_add(&allocator, &order, p);

    game_state_t game = game_init(&allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    test_click_tile(&game, &allocator, p->position);
    test_click_tile(&game, &allocator, (position_t){5, 0});

    entity_t *entity = p;
    assert_test(entity->position.x == 0 && entity->position.y == 0);
    assert_test(entity->mp == 1);

    game_deinit(&allocator, game);
}

static void test_game_end_turn_advances_past_a_harmless_enemy_and_deselects(void) {
    static char buffer[8192];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_t grid_padding = grid_align(&allocator);
    grid_t grid = grid_init(&allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_t entity_list_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(&allocator);
    entity_t* p = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* e = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){15, 9}, 10, 2, 0); // far away, zero mp: can't reach or attack

    slice_t turn_order_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(&allocator);
    turn_order_add(&allocator, &order, p);
    turn_order_add(&allocator, &order, e);

    game_state_t game = game_init(&allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    p->ap = 0;
    p->mp = 0;

    test_click_tile(&game, &allocator, p->position);
    assert_test(game.selected_entity == p);

    test_click_end_turn(&game, &allocator);

    // e's turn happened (harmlessly) and the cursor wrapped back to p.
    assert_test(turn_active_entity(game.turn) == p);
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

    slice_t grid_padding = grid_align(&allocator);
    grid_t grid = grid_init(&allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_t entity_list_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(&allocator);
    entity_t* p = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* e = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 5, 2, 3);

    slice_t turn_order_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(&allocator);
    turn_order_add(&allocator, &order, p);
    turn_order_add(&allocator, &order, e);

    game_state_t game = game_init(&allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    test_click_tile(&game, &allocator, p->position);
    test_click_tile(&game, &allocator, e->position);

    assert_test(!e->alive);
    assert_test(game.game_over == GAME_OVER_WIN);

    // Further presses of any kind must now be frozen no-ops.
    test_click_tile(&game, &allocator, (position_t){2, 0});
    entity_t *player = p;
    assert_test(player->position.x == 0 && player->position.y == 0);

    entity_t *active_before = turn_active_entity(game.turn);
    test_click_end_turn(&game, &allocator);
    assert_test(turn_active_entity(game.turn) == active_before);

    assert_test(game.game_over == GAME_OVER_WIN);

    game_deinit(&allocator, game);
}

static void test_game_ai_kills_last_player_during_end_turn_sets_lose(void) {
    static char buffer[8192];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_t grid_padding = grid_align(&allocator);
    grid_t grid = grid_init(&allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_t entity_list_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(&allocator);
    entity_t* p = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 5, 2, 3);
    entity_t* e = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 10, 2, 3);

    slice_t turn_order_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(&allocator);
    turn_order_add(&allocator, &order, p);
    turn_order_add(&allocator, &order, e);

    game_state_t game = game_init(&allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    assert_test(turn_active_entity(game.turn) == p);

    test_click_end_turn(&game, &allocator);

    assert_test(!p->alive);
    assert_test(game.game_over == GAME_OVER_LOSE);

    // Further presses must be frozen no-ops.
    entity_t *active_before = turn_active_entity(game.turn);
    test_click_end_turn(&game, &allocator);
    assert_test(turn_active_entity(game.turn) == active_before);

    game_deinit(&allocator, game);
}

static void test_game_on_input_event_click_in_end_turn_button_behaves_like_end_turn_pressed(void) {
    static char buffer[8192];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_t grid_padding = grid_align(&allocator);
    grid_t grid = grid_init(&allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_t entity_list_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(&allocator);
    entity_t* p = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* e = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){15, 9}, 10, 2, 0); // far away, zero mp: can't reach or attack

    slice_t turn_order_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(&allocator);
    turn_order_add(&allocator, &order, p);
    turn_order_add(&allocator, &order, e);

    game_state_t game = game_init(&allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    p->ap = 0;
    p->mp = 0;

    test_click_tile(&game, &allocator, p->position);
    assert_test(game.selected_entity == p);

    assert_test(point_in_rect(game.viewport.end_turn_button, 260, 215));
    input_event_t click = { .type = INPUT_EVENT_MOUSE_CLICK, .x = 260, .y = 215 };
    game_on_input_event(&game, &allocator, click);

    assert_test(turn_active_entity(game.turn) == p);
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

    slice_t grid_padding = grid_align(&allocator);
    grid_t grid = grid_init(&allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_t entity_list_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(&allocator);
    entity_t* p1 = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){3, 3}, 10, 2, 3);

    slice_t turn_order_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(&allocator);
    turn_order_add(&allocator, &order, p1);

    game_state_t game = game_init(&allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    int px, py;
    grid_to_screen(game.viewport, 0, 0, &px, &py);

    input_event_t click = { .type = INPUT_EVENT_MOUSE_CLICK, .x = px + 1, .y = py + 1 };
    game_on_input_event(&game, &allocator, click);

    assert_test(game.selected_entity == p1);

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

    // Turn order alternates player/enemy: p1, e1, p2, e2, p3, e3.
    int expected_order_ids[6] = { 0, 3, 1, 4, 2, 5 };
    assert_test(SLICE_TYPESIZE(game.turn.order) == 6);
    for (int i = 0; i < 6; i++) {
        assert_test(SLICE_AT(game.turn.order, i) == &SLICE_AT(game.entities, expected_order_ids[i]));
    }
    assert_test(turn_active_entity(game.turn) == &SLICE_AT(game.entities, 0));

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
    { TEST_NAME("layout_compute_defaults"), test_layout_compute_defaults },
    { TEST_NAME("point_in_rect"), test_point_in_rect },
    { TEST_NAME("screen_to_grid_corners"), test_screen_to_grid_corners },
    { TEST_NAME("grid_to_screen_round_trips_with_screen_to_grid"), test_grid_to_screen_round_trips_with_screen_to_grid },
    { TEST_NAME("game_selecting_entity_computes_reachable_tiles_within_mp_and_moves"), test_game_selecting_entity_computes_reachable_tiles_within_mp_and_moves },
    { TEST_NAME("game_obstacles_block_reachable_tiles_and_movement"), test_game_obstacles_block_reachable_tiles_and_movement },
    { TEST_NAME("game_occupied_tile_blocks_corridor_reachability"), test_game_occupied_tile_blocks_corridor_reachability },
    { TEST_NAME("game_attack_kills_defender_clamps_hp_and_frees_tile_for_movement"), test_game_attack_kills_defender_clamps_hp_and_frees_tile_for_movement },
    { TEST_NAME("game_entity_pressed_diagonal_and_far_enemy_attack_noop"), test_game_entity_pressed_diagonal_and_far_enemy_attack_noop },
    { TEST_NAME("game_turn_order_compacts_when_non_active_entity_dies_during_attack"), test_game_turn_order_compacts_when_non_active_entity_dies_during_attack },
    { TEST_NAME("game_ai_adjacent_enemy_attacks_without_moving_on_end_turn"), test_game_ai_adjacent_enemy_attacks_without_moving_on_end_turn },
    { TEST_NAME("game_ai_far_enemy_with_enough_mp_closes_and_attacks_on_end_turn"), test_game_ai_far_enemy_with_enough_mp_closes_and_attacks_on_end_turn },
    { TEST_NAME("game_ai_insufficient_mp_moves_partial_no_attack_on_end_turn"), test_game_ai_insufficient_mp_moves_partial_no_attack_on_end_turn },
    { TEST_NAME("game_ai_obstacle_forces_detour_on_end_turn"), test_game_ai_obstacle_forces_detour_on_end_turn },
    { TEST_NAME("game_ai_multiple_enemies_act_independently_on_end_turn"), test_game_ai_multiple_enemies_act_independently_on_end_turn },
    { TEST_NAME("game_ai_zero_mp_not_adjacent_does_nothing_on_end_turn"), test_game_ai_zero_mp_not_adjacent_does_nothing_on_end_turn },
    { TEST_NAME("game_entity_pressed_selects_only_the_active_entity"), test_game_entity_pressed_selects_only_the_active_entity },
    { TEST_NAME("game_entity_pressed_enemy_active_noops"), test_game_entity_pressed_enemy_active_noops },
    { TEST_NAME("game_entity_pressed_adjacent_enemy_attacks_then_noops_when_ap_zero"), test_game_entity_pressed_adjacent_enemy_attacks_then_noops_when_ap_zero },
    { TEST_NAME("game_tile_pressed_moves_within_reach_and_consumes_mp"), test_game_tile_pressed_moves_within_reach_and_consumes_mp },
    { TEST_NAME("game_tile_pressed_noops_on_unreachable_tile"), test_game_tile_pressed_noops_on_unreachable_tile },
    { TEST_NAME("game_end_turn_advances_past_a_harmless_enemy_and_deselects"), test_game_end_turn_advances_past_a_harmless_enemy_and_deselects },
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
