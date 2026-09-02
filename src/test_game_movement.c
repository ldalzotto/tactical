#include "test_game_movement.h"
#include "game/game.h"
#include "game/pathing.h"
#include "game/position.h"
#include "lib/assert.h"
#include "game/entity.h"
#include "game/skill.h"
#include "game/grid.h"
#include "game/turn.h"
#include "lib/linkage.h"
#include "lib/memory.h"
#include "test.h"
#include "test_game_helpers.h"
#include <stdint.h>

// These tests drive game subsystems only through game_on_input_event (via
// test_click_tile/test_click_end_turn) and assert on game_state_t fields,
// same as a player would. Behavior the UI structurally prevents a player
// from triggering has no test here -- there's no click that reaches it.

PRIVATE void test_game_selecting_entity_computes_reachable_tiles_within_mp_and_moves(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 2);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = skills;

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    assert_test(game.mode == GAME_MODE_MOVEMENT);
    assert_test(turn_active_entity(game.turn) == p);

    // mp is 2: (0,0) is dist 0 (mover's own tile, excluded), (2,0) is dist 2
    // (in range), (3,0)/(3,3) are dist 3/6 (beyond mp) -- checks both BFS
    // distance and the max-steps cap.
    assert_test(!test_position_reachable(&game, (position_t){0, 0}));
    assert_test(test_position_reachable(&game, (position_t){2, 0}));
    assert_test(!test_position_reachable(&game, (position_t){3, 0}));
    assert_test(!test_position_reachable(&game, (position_t){3, 3}));

    test_click_tile(&game, allocator, (position_t){2, 0});

    entity_t *entity = p;
    assert_test(entity->position.x == 2);
    assert_test(entity->position.y == 0);
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

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = skills;

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);

    assert_test(!test_position_reachable(&game, (position_t){2, 0}));
    assert_test(!test_position_reachable(&game, (position_t){2, 1}));
    assert_test(test_position_reachable(&game, (position_t){1, 1}));
    // (4,1) is past the wall: only in range if BFS took the 6-tile
    // walk-around (direct 4-tile path is blocked), so must be absent.
    assert_test(!test_position_reachable(&game, (position_t){4, 1}));

    // Clicking the wall is a no-op: unwalkable tiles are never a valid target.
    test_click_tile(&game, allocator, (position_t){2, 0});

    entity_t *entity = p;
    assert_test(entity->position.x == 0);
    assert_test(entity->position.y == 1);
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

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *blocker_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    blocker->skills = (slice_skill_t){ .begin = blocker_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, blocker);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);

    assert_test(test_position_reachable(&game, (position_t){1, 0}));
    // The occupied tile and everything past it are unreachable: the living
    // blocker seals the corridor despite ample mp.
    assert_test(!test_position_reachable(&game, (position_t){2, 0}));
    assert_test(!test_position_reachable(&game, (position_t){3, 0}));
    assert_test(!test_position_reachable(&game, (position_t){4, 0}));

    game_deinit(allocator, game);
}

PRIVATE void test_game_tile_pressed_moves_within_reach_and_consumes_mp(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = skills;

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    test_click_tile(&game, allocator, p->position);
    test_click_tile(&game, allocator, (position_t){2, 0});

    entity_t *entity = p;
    assert_test(entity->position.x == 2);
    assert_test(entity->position.y == 0);
    assert_test(entity->mp == 1);

    game_deinit(allocator, game);
}

PRIVATE void test_game_tile_pressed_noops_on_unreachable_tile(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 1);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = skills;

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    test_click_tile(&game, allocator, p->position);
    test_click_tile(&game, allocator, (position_t){5, 0});

    entity_t *entity = p;
    assert_test(entity->position.x == 0);
    assert_test(entity->position.y == 0);
    assert_test(entity->mp == 1);

    game_deinit(allocator, game);
}

// Fresh 4x4 single-player scenario, one instance per target tile since
// each click in test_game_reachable_tiles_match_what_action_try_move_accepts
// consumes mp and moves the entity.
PRIVATE game_state_t test_e2e_movement_scenario(linear_allocator_t *allocator, entity_t **out_p) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 2);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = skills;

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);
    *out_p = p;
    return game;
}

// End-to-end: the reachable-tiles overlay (walking_distances >= 1, what
// render.c shows) and action_try_move (what execution accepts) agree --
// every tile the overlay marks reachable can actually be clicked, and
// deducts exactly the mp the cache reports.
PRIVATE void test_game_reachable_tiles_match_what_action_try_move_accepts(linear_allocator_t *allocator) {
    entity_t *p;
    game_state_t game = test_e2e_movement_scenario(allocator, &p);

    test_click_tile(&game, allocator, p->position);
    assert_test(game.mode == GAME_MODE_MOVEMENT);

    position_t targets[16];
    int expected_distance[16];
    int count = 0;
    for (int ty = 0; ty < game.grid.height; ty++) {
        for (int tx = 0; tx < game.grid.width; tx++) {
            position_t position = { tx, ty };
            int distance = pathing_distance_at(game.pathing.walking_distances, game.grid, position);
            if (distance >= 1) {
                assert_test(count < 16);
                targets[count] = position;
                expected_distance[count] = distance;
                count++;
            }
        }
    }
    assert_test(count > 0);

    game_deinit(allocator, game);

    for (int i = 0; i < count; i++) {
        entity_t *fresh_p;
        game_state_t fresh_game = test_e2e_movement_scenario(allocator, &fresh_p);

        test_click_tile(&fresh_game, allocator, fresh_p->position);
        int starting_mp = fresh_p->mp;

        test_click_tile(&fresh_game, allocator, targets[i]);

        assert_test(position_equals(fresh_p->position, targets[i]));
        assert_test(fresh_p->mp == starting_mp - expected_distance[i]);

        game_deinit(allocator, fresh_game);
    }
}

const test_case_t g_game_movement_tests[] = {
    { TEST_NAME("game_selecting_entity_computes_reachable_tiles_within_mp_and_moves"), test_game_selecting_entity_computes_reachable_tiles_within_mp_and_moves },
    { TEST_NAME("game_obstacles_block_reachable_tiles_and_movement"), test_game_obstacles_block_reachable_tiles_and_movement },
    { TEST_NAME("game_occupied_tile_blocks_corridor_reachability"), test_game_occupied_tile_blocks_corridor_reachability },
    { TEST_NAME("game_tile_pressed_moves_within_reach_and_consumes_mp"), test_game_tile_pressed_moves_within_reach_and_consumes_mp },
    { TEST_NAME("game_tile_pressed_noops_on_unreachable_tile"), test_game_tile_pressed_noops_on_unreachable_tile },
    { TEST_NAME("game_reachable_tiles_match_what_action_try_move_accepts"), test_game_reachable_tiles_match_what_action_try_move_accepts },
};

const uint32_t g_game_movement_tests_count = sizeof(g_game_movement_tests) / sizeof(g_game_movement_tests[0]);
