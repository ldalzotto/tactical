#include "test_game_movement.h"
#include "lib/assert.h"
#include "game/entity.h"
#include "game/grid.h"
#include "game/turn.h"
#include "test_game_helpers.h"

// The tests below drive game subsystems (grid, entity, pathing, turn,
// action, ai) only through game_init's public surface -- game_on_input_event
// via test_click_tile/test_click_end_turn -- and assert on game_state_t
// fields, the same way a player and a screen reader of the board would.
// Behavior that the game API structurally prevents a player from ever
// triggering (attacking a target the UI won't let you select, moving onto a
// tile the UI routes to a different handler) has no equivalent test here:
// there's no click that reaches it.

PRIVATE void test_game_selecting_entity_computes_reachable_tiles_within_mp_and_moves(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 2);

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
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

    test_click_tile(&game, allocator, (position_t){2, 0});

    entity_t *entity = p;
    assert_test(entity->position.x == 2 && entity->position.y == 0);
    assert_test(entity->mp == 0);

    game_deinit(allocator, game);
}

PRIVATE void test_game_obstacles_block_reachable_tiles_and_movement(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 5, 3);
    grid_set_walkable(grid, (position_t){2, 0}, false);
    grid_set_walkable(grid, (position_t){2, 1}, false);

    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 1}, 10, 2, 4);

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);

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
    test_click_tile(&game, allocator, (position_t){2, 0});

    entity_t *entity = p;
    assert_test(entity->position.x == 0 && entity->position.y == 1);
    assert_test(entity->mp == 4);

    game_deinit(allocator, game);
}

PRIVATE void test_game_occupied_tile_blocks_corridor_reachability(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 5, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 10);
    entity_t* blocker = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){2, 0}, 10, 2, 3);

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, blocker);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);

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

    game_deinit(allocator, game);
}

PRIVATE void test_game_tile_pressed_moves_within_reach_and_consumes_mp(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    test_click_tile(&game, allocator, p->position);
    test_click_tile(&game, allocator, (position_t){2, 0});

    entity_t *entity = p;
    assert_test(entity->position.x == 2 && entity->position.y == 0);
    assert_test(entity->mp == 1);

    game_deinit(allocator, game);
}

PRIVATE void test_game_tile_pressed_noops_on_unreachable_tile(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 1);

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    test_click_tile(&game, allocator, p->position);
    test_click_tile(&game, allocator, (position_t){5, 0});

    entity_t *entity = p;
    assert_test(entity->position.x == 0 && entity->position.y == 0);
    assert_test(entity->mp == 1);

    game_deinit(allocator, game);
}

const test_case_t g_game_movement_tests[] = {
    { TEST_NAME("game_selecting_entity_computes_reachable_tiles_within_mp_and_moves"), test_game_selecting_entity_computes_reachable_tiles_within_mp_and_moves },
    { TEST_NAME("game_obstacles_block_reachable_tiles_and_movement"), test_game_obstacles_block_reachable_tiles_and_movement },
    { TEST_NAME("game_occupied_tile_blocks_corridor_reachability"), test_game_occupied_tile_blocks_corridor_reachability },
    { TEST_NAME("game_tile_pressed_moves_within_reach_and_consumes_mp"), test_game_tile_pressed_moves_within_reach_and_consumes_mp },
    { TEST_NAME("game_tile_pressed_noops_on_unreachable_tile"), test_game_tile_pressed_noops_on_unreachable_tile },
};

const uint32_t g_game_movement_tests_count = sizeof(g_game_movement_tests) / sizeof(g_game_movement_tests[0]);
