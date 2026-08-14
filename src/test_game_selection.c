#include "test_game_selection.h"
#include "lib/assert.h"
#include "game/entity.h"
#include "game/skill.h"
#include "game/grid.h"
#include "game/turn.h"
#include "game/layout.h"
#include "test_game_helpers.h"

PRIVATE void test_game_entity_pressed_selects_only_the_active_entity(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p1 = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3, SKILL_MELEE);
    entity_t* p2 = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){1, 0}, 10, 2, 3, SKILL_MELEE);

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p1);
    turn_order_add(allocator, &order, p2);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);
    assert_test(turn_active_entity(game.turn) == p1);

    test_click_tile(&game, allocator, p1->position);
    assert_test(game.selected_entity == p1);

    // p2 isn't the active entity: pressing it is a no-op, including as an
    // attack target -- same-team damage never lands.
    test_click_tile(&game, allocator, p2->position);
    assert_test(game.selected_entity == p1);
    assert_test(p1->ap == 2);
    assert_test(p2->hp == 10);

    game_deinit(allocator, game);
}

PRIVATE void test_game_entity_pressed_enemy_active_noops(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* e1 = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){5, 5}, 10, 2, 3, SKILL_MELEE);

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, e1);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    test_click_tile(&game, allocator, e1->position);
    assert_test(game.selected_entity == 0);

    game_deinit(allocator, game);
}

PRIVATE void test_game_end_turn_advances_past_a_harmless_enemy_and_deselects(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3, SKILL_MELEE);
    entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){15, 9}, 10, 2, 0, SKILL_MELEE); // far away, zero mp: can't reach or attack

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, e);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    p->ap = 0;
    p->mp = 0;

    test_click_tile(&game, allocator, p->position);
    assert_test(game.selected_entity == p);

    test_click_end_turn(&game, allocator);

    // e's turn happened (harmlessly) and the cursor wrapped back to p.
    assert_test(turn_active_entity(game.turn) == p);
    assert_test(game.selected_entity == 0);

    entity_t *player = p;
    assert_test(player->ap == player->max_ap);
    assert_test(player->mp == player->max_mp);
    assert_test(player->alive);

    game_deinit(allocator, game);
}

PRIVATE void test_game_1v1_enemy_death_sets_win_and_freezes_input(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3, SKILL_MELEE);
    entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 5, 2, 3, SKILL_MELEE);

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, e);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);
    test_click_tile(&game, allocator, e->position);

    assert_test(!e->alive);
    assert_test(game.game_over == GAME_OVER_WIN);

    // Further presses of any kind must now be frozen no-ops.
    test_click_tile(&game, allocator, (position_t){2, 0});
    entity_t *player = p;
    assert_test(player->position.x == 0 && player->position.y == 0);

    entity_t *active_before = turn_active_entity(game.turn);
    test_click_end_turn(&game, allocator);
    assert_test(turn_active_entity(game.turn) == active_before);

    assert_test(game.game_over == GAME_OVER_WIN);

    game_deinit(allocator, game);
}

PRIVATE void test_game_ai_kills_last_player_during_end_turn_sets_lose(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 5, 2, 3, SKILL_MELEE);
    entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 10, 2, 3, SKILL_MELEE);

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, e);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    assert_test(turn_active_entity(game.turn) == p);

    test_click_end_turn(&game, allocator);

    assert_test(!p->alive);
    assert_test(game.game_over == GAME_OVER_LOSE);

    // Further presses must be frozen no-ops.
    entity_t *active_before = turn_active_entity(game.turn);
    test_click_end_turn(&game, allocator);
    assert_test(turn_active_entity(game.turn) == active_before);

    game_deinit(allocator, game);
}

PRIVATE void test_game_on_input_event_click_in_end_turn_button_behaves_like_end_turn_pressed(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3, SKILL_MELEE);
    entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){15, 9}, 10, 2, 0, SKILL_MELEE); // far away, zero mp: can't reach or attack

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, e);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    p->ap = 0;
    p->mp = 0;

    test_click_tile(&game, allocator, p->position);
    assert_test(game.selected_entity == p);

    assert_test(point_in_rect(game.viewport.end_turn_button, 260, 215));
    input_event_t click = { .type = INPUT_EVENT_MOUSE_CLICK, .x = 260, .y = 215 };
    game_on_input_event(&game, allocator, click);

    assert_test(turn_active_entity(game.turn) == p);
    assert_test(game.selected_entity == 0);

    entity_t *player = p;
    assert_test(player->ap == player->max_ap);
    assert_test(player->mp == player->max_mp);

    game_deinit(allocator, game);
}

PRIVATE void test_game_on_input_event_click_on_entity_tile_behaves_like_entity_pressed(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p1 = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3, SKILL_MELEE);
    entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){3, 3}, 10, 2, 3, SKILL_MELEE);

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p1);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    int px, py;
    grid_to_screen(game.viewport, 0, 0, &px, &py);

    input_event_t click = { .type = INPUT_EVENT_MOUSE_CLICK, .x = px + 1, .y = py + 1 };
    game_on_input_event(&game, allocator, click);

    assert_test(game.selected_entity == p1);

    game_deinit(allocator, game);
}

const test_case_t g_game_selection_tests[] = {
    { TEST_NAME("game_entity_pressed_selects_only_the_active_entity"), test_game_entity_pressed_selects_only_the_active_entity },
    { TEST_NAME("game_entity_pressed_enemy_active_noops"), test_game_entity_pressed_enemy_active_noops },
    { TEST_NAME("game_end_turn_advances_past_a_harmless_enemy_and_deselects"), test_game_end_turn_advances_past_a_harmless_enemy_and_deselects },
    { TEST_NAME("game_1v1_enemy_death_sets_win_and_freezes_input"), test_game_1v1_enemy_death_sets_win_and_freezes_input },
    { TEST_NAME("game_ai_kills_last_player_during_end_turn_sets_lose"), test_game_ai_kills_last_player_during_end_turn_sets_lose },
    { TEST_NAME("game_on_input_event_click_in_end_turn_button_behaves_like_end_turn_pressed"), test_game_on_input_event_click_in_end_turn_button_behaves_like_end_turn_pressed },
    { TEST_NAME("game_on_input_event_click_on_entity_tile_behaves_like_entity_pressed"), test_game_on_input_event_click_on_entity_tile_behaves_like_entity_pressed },
};

const uint32_t g_game_selection_tests_count = sizeof(g_game_selection_tests) / sizeof(g_game_selection_tests[0]);
