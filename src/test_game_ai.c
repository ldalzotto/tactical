#include "test_game_ai.h"
#include "lib/assert.h"
#include "game/entity.h"
#include "game/skill.h"
#include "game/grid.h"
#include "game/turn.h"
#include "test_game_helpers.h"

PRIVATE void test_game_ai_adjacent_enemy_attacks_without_moving_on_end_turn(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3, SKILL_MELEE);
    entity_t* enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 10, 2, 3, SKILL_MELEE);

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, enemy);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, 320, 240, 40);

    test_click_end_turn(&game, allocator);

    assert_test(turn_active_entity(game.turn) == p);
    assert_test(enemy->position.x == 1 && enemy->position.y == 0);
    assert_test(enemy->ap == 1);
    assert_test(p->hp == 5);
    assert_test(p->alive);

    game_deinit(allocator, game);
}

PRIVATE void test_game_ai_far_enemy_with_enough_mp_closes_and_attacks_on_end_turn(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 5, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3, SKILL_MELEE);
    entity_t* enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){4, 0}, 10, 2, 4, SKILL_MELEE);

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, enemy);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, 320, 240, 40);

    test_click_end_turn(&game, allocator);

    assert_test(turn_active_entity(game.turn) == p);
    assert_test(enemy->position.x == 1 && enemy->position.y == 0);
    assert_test(enemy->mp == 1);
    assert_test(enemy->ap == 1);
    assert_test(p->hp == 5);
    assert_test(p->alive);

    game_deinit(allocator, game);
}

PRIVATE void test_game_ai_insufficient_mp_moves_partial_no_attack_on_end_turn(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 5, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3, SKILL_MELEE);
    entity_t* enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){4, 0}, 10, 2, 1, SKILL_MELEE);

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, enemy);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, 320, 240, 40);

    test_click_end_turn(&game, allocator);

    assert_test(turn_active_entity(game.turn) == p);
    assert_test(enemy->position.x == 3 && enemy->position.y == 0);
    assert_test(enemy->mp == 0);
    assert_test(enemy->ap == 2);
    assert_test(!skill_target_in_range(allocator, grid, entities, enemy, p));
    assert_test(p->hp == 10);

    game_deinit(allocator, game);
}

PRIVATE void test_game_ai_obstacle_forces_detour_on_end_turn(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 5, 3);
    grid_set_walkable(grid, (position_t){2, 0}, false);
    grid_set_walkable(grid, (position_t){2, 1}, false);

    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){4, 1}, 10, 2, 3, SKILL_MELEE);
    entity_t* enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){0, 1}, 10, 2, 8, SKILL_MELEE);

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, enemy);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, 320, 240, 40);

    test_click_end_turn(&game, allocator);

    assert_test(turn_active_entity(game.turn) == p);
    assert_test(skill_target_in_range(allocator, grid, entities, enemy, p));
    assert_test(p->hp == 5);
    assert_test(enemy->ap == 1);

    game_deinit(allocator, game);
}

PRIVATE void test_game_ai_multiple_enemies_act_independently_on_end_turn(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 30, 2, 3, SKILL_MELEE);
    entity_t* enemy_a = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 0}, 10, 2, 3, SKILL_MELEE);
    entity_t* enemy_b = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){0, 3}, 10, 2, 3, SKILL_MELEE);

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, enemy_a);
    turn_order_add(allocator, &order, enemy_b);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, 320, 240, 40);

    // One end-turn press plays out both enemies' turns in order, back
    // around to the player.
    test_click_end_turn(&game, allocator);

    assert_test(turn_active_entity(game.turn) == p);

    assert_test(enemy_a->position.x == 1 && enemy_a->position.y == 0);
    assert_test(enemy_a->mp == 1);
    assert_test(enemy_a->ap == 1);

    assert_test(enemy_b->position.x == 0 && enemy_b->position.y == 1);
    assert_test(enemy_b->mp == 1);
    assert_test(enemy_b->ap == 1);

    assert_test(p->hp == 20);
    assert_test(p->alive);

    game_deinit(allocator, game);
}

PRIVATE void test_game_ai_zero_mp_not_adjacent_does_nothing_on_end_turn(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3, SKILL_MELEE);
    entity_t* enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 3}, 10, 2, 0, SKILL_MELEE);

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, enemy);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, 320, 240, 40);

    test_click_end_turn(&game, allocator);

    assert_test(turn_active_entity(game.turn) == p);
    assert_test(enemy->position.x == 3 && enemy->position.y == 3);
    assert_test(enemy->mp == 0);
    assert_test(enemy->ap == 2);
    assert_test(p->hp == 10);

    game_deinit(allocator, game);
}

PRIVATE void test_game_ai_ranged_enemy_attacks_from_range_without_closing_on_end_turn(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3, SKILL_MELEE);
    entity_t* enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 0}, 10, 2, 3, SKILL_RANGED);

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, enemy);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, 320, 240, 40);

    test_click_end_turn(&game, allocator);

    assert_test(turn_active_entity(game.turn) == p);
    // Already within SKILL_RANGED.range (3): no need to move in.
    assert_test(enemy->position.x == 3 && enemy->position.y == 0);
    assert_test(enemy->mp == 3);
    assert_test(enemy->ap == 1);
    assert_test(p->hp == 7);
    assert_test(p->alive);

    game_deinit(allocator, game);
}

const test_case_t g_game_ai_tests[] = {
    { TEST_NAME("game_ai_adjacent_enemy_attacks_without_moving_on_end_turn"), test_game_ai_adjacent_enemy_attacks_without_moving_on_end_turn },
    { TEST_NAME("game_ai_far_enemy_with_enough_mp_closes_and_attacks_on_end_turn"), test_game_ai_far_enemy_with_enough_mp_closes_and_attacks_on_end_turn },
    { TEST_NAME("game_ai_insufficient_mp_moves_partial_no_attack_on_end_turn"), test_game_ai_insufficient_mp_moves_partial_no_attack_on_end_turn },
    { TEST_NAME("game_ai_obstacle_forces_detour_on_end_turn"), test_game_ai_obstacle_forces_detour_on_end_turn },
    { TEST_NAME("game_ai_multiple_enemies_act_independently_on_end_turn"), test_game_ai_multiple_enemies_act_independently_on_end_turn },
    { TEST_NAME("game_ai_zero_mp_not_adjacent_does_nothing_on_end_turn"), test_game_ai_zero_mp_not_adjacent_does_nothing_on_end_turn },
    { TEST_NAME("game_ai_ranged_enemy_attacks_from_range_without_closing_on_end_turn"), test_game_ai_ranged_enemy_attacks_from_range_without_closing_on_end_turn },
};

const uint32_t g_game_ai_tests_count = sizeof(g_game_ai_tests) / sizeof(g_game_ai_tests[0]);
