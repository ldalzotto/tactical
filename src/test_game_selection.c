#include "test_game_selection.h"
#include "game/game.h"
#include "game/position.h"
#include "game/ui.h"
#include "lib/assert.h"
#include "game/entity.h"
#include "game/skill.h"
#include "game/grid.h"
#include "game/turn.h"
#include "game/layout.h"
#include "lib/linkage.h"
#include "lib/memory.h"
#include "test.h"
#include "test_game_helpers.h"
#include <stdint.h>

PRIVATE void test_game_entity_pressed_selects_only_the_active_entity(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p1 = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* p2 = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){1, 0}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p1_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p1->skills = (slice_skill_t){ .begin = p1_skills_begin, .end = skills.end };
    skill_t *p2_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p2->skills = (slice_skill_t){ .begin = p2_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p1);
    turn_order_add(allocator, &order, p2);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);
    assert_test(turn_active_entity(game.turn) == p1);

    test_click_tile(&game, allocator, p1->position);
    assert_test(game.mode == GAME_MODE_MOVEMENT);
    assert_test(turn_active_entity(game.turn) == p1);

    // p2 isn't active: pressing it is a no-op, even as an attack target.
    test_click_tile(&game, allocator, p2->position);
    assert_test(game.mode == GAME_MODE_MOVEMENT);
    assert_test(turn_active_entity(game.turn) == p1);
    assert_test(p1->ap == 2);
    assert_test(p2->hp == 10);

    game_deinit(allocator, game);
}

PRIVATE void test_game_entity_pressed_enemy_active_noops(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* e1 = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){5, 5}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e1->skills = skills;

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, e1);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    test_click_tile(&game, allocator, e1->position);
    assert_test(game.mode == GAME_MODE_NONE);

    game_deinit(allocator, game);
}

PRIVATE void test_game_end_turn_advances_past_a_harmless_enemy_and_deselects(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){15, 9}, 10, 2, 0); // far away, zero mp: can't reach or attack

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *e_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e->skills = (slice_skill_t){ .begin = e_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, e);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    p->ap = 0;
    p->mp = 0;

    test_click_tile(&game, allocator, p->position);
    assert_test(game.mode == GAME_MODE_MOVEMENT);
    assert_test(turn_active_entity(game.turn) == p);

    test_click_end_turn(&game, allocator);

    // e's turn happened (harmlessly) and the cursor wrapped back to p.
    assert_test(turn_active_entity(game.turn) == p);
    assert_test(game.mode == GAME_MODE_NONE);

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
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 5, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *e_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e->skills = (slice_skill_t){ .begin = e_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, e);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);
    test_click_tile(&game, allocator, e->position);

    assert_test(!e->alive);
    assert_test(game.game_over == GAME_OVER_WIN);

    // Further presses of any kind must now be frozen no-ops.
    test_click_tile(&game, allocator, (position_t){2, 0});
    entity_t *player = p;
    assert_test(player->position.x == 0);
    assert_test(player->position.y == 0);

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
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 5, 2, 3);
    entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *e_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e->skills = (slice_skill_t){ .begin = e_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, e);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

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
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){15, 9}, 10, 2, 0); // far away, zero mp: can't reach or attack

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *e_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e->skills = (slice_skill_t){ .begin = e_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, e);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    p->ap = 0;
    p->mp = 0;

    test_click_tile(&game, allocator, p->position);
    assert_test(game.mode == GAME_MODE_MOVEMENT);
    assert_test(turn_active_entity(game.turn) == p);

    assert_test(point_in_rect(game.viewport.end_turn_button, 260, 215));
    test_click_end_turn(&game, allocator);

    assert_test(turn_active_entity(game.turn) == p);
    assert_test(game.mode == GAME_MODE_NONE);

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
    entity_t* p1 = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* p2 = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){3, 3}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p1_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p1->skills = (slice_skill_t){ .begin = p1_skills_begin, .end = skills.end };
    skill_t *p2_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p2->skills = (slice_skill_t){ .begin = p2_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p1);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    test_click_tile(&game, allocator, p1->position);

    assert_test(game.mode == GAME_MODE_MOVEMENT);
    assert_test(turn_active_entity(game.turn) == p1);

    game_deinit(allocator, game);
}

PRIVATE void test_game_tile_pressed_enemy_active_noops(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){2, 2}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e->skills = skills;

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, e);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    // Empty tile press while an enemy is active: the movement handler
    // ignores it.
    test_click_tile(&game, allocator, (position_t){0, 0});
    assert_test(game.mode == GAME_MODE_NONE);
    assert_test(e->position.x == 2);
    assert_test(e->position.y == 2);
    assert_test(e->mp == 3);

    game_deinit(allocator, game);
}

PRIVATE void test_game_tile_pressed_mode_none_noops(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);
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

    // No selection made yet: an empty-tile click is a no-op.
    test_click_tile(&game, allocator, (position_t){2, 0});
    assert_test(game.mode == GAME_MODE_NONE);
    assert_test(p->position.x == 0);
    assert_test(p->position.y == 0);
    assert_test(p->mp == 3);

    game_deinit(allocator, game);
}

PRIVATE void test_game_attack_toggle_enemy_active_noops(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){0, 0}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e->skills = skills;

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, e);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    test_click_attack_toggle(&game, allocator);
    assert_test(game.mode == GAME_MODE_NONE);

    game_deinit(allocator, game);
}

PRIVATE void test_game_attack_toggle_mode_none_noops(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);
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

    test_click_attack_toggle(&game, allocator);
    assert_test(game.mode == GAME_MODE_NONE);

    game_deinit(allocator, game);
}

PRIVATE void test_game_end_turn_enemy_active_noops(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){0, 0}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e->skills = skills;

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, e);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    test_click_end_turn(&game, allocator);
    assert_test(game.mode == GAME_MODE_NONE);
    assert_test(turn_active_entity(game.turn) == e);

    game_deinit(allocator, game);
}

PRIVATE void test_game_mouse_move_updates_hover(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);
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

    test_move_tile(&game, allocator, (position_t){2, 2});
    assert_test(game.hover_valid);
    assert_test(game.hover.x == 2);
    assert_test(game.hover.y == 2);

    test_move_to_pixel(&game, allocator, 9999, 9999);
    assert_test(!game.hover_valid);

    game_deinit(allocator, game);
}

PRIVATE void test_game_skill_button_hit_test_clamps_more_than_two_skills(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    skill_list_add(allocator, &skills, SKILL_RANGED);
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    test_click_tile(&game, allocator, p->position);
    assert_test(game.mode == GAME_MODE_MOVEMENT);

    // With 3 skills, the hit-test gate must clamp to
    // VIEWPORT_MAX_SKILL_BUTTONS (2); re-clicking the active entity
    // re-enters movement mode without a crash or skill change.
    test_click_tile(&game, allocator, p->position);
    assert_test(game.mode == GAME_MODE_MOVEMENT);
    assert_test(game.selected_skill == 0);

    game_deinit(allocator, game);
}

PRIVATE void test_game_key_down_selects_visible_skill(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    skill_list_add(allocator, &skills, SKILL_RANGED);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    test_click_tile(&game, allocator, p->position);
    assert_test(game.mode == GAME_MODE_MOVEMENT);
    assert_test(game.selected_skill == 0);

    // Key '2' selects the 2nd visible skill button (index 1).
    test_press_key(&game, allocator, '2');
    assert_test(game.selected_skill == 1);

    game_deinit(allocator, game);
}

PRIVATE void test_game_key_down_out_of_range_digit_noops(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    skill_list_add(allocator, &skills, SKILL_RANGED);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    test_click_tile(&game, allocator, p->position);
    assert_test(game.mode == GAME_MODE_MOVEMENT);

    // Only 2 skills -> only 2 buttons visible; '3' has nothing to hit.
    test_press_key(&game, allocator, '3');
    assert_test(game.selected_skill == 0);
    assert_test(game.mode == GAME_MODE_MOVEMENT);

    // Non-digit key is also a no-op, both below and above the '1'..'9' range.
    test_press_key(&game, allocator, '0');
    assert_test(game.selected_skill == 0);
    assert_test(game.mode == GAME_MODE_MOVEMENT);

    test_press_key(&game, allocator, 'a');
    assert_test(game.selected_skill == 0);
    assert_test(game.mode == GAME_MODE_MOVEMENT);

    game_deinit(allocator, game);
}

PRIVATE void test_game_key_down_enemy_active_noops(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){0, 0}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *e_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    skill_list_add(allocator, &skills, SKILL_RANGED);
    e->skills = (slice_skill_t){ .begin = e_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, e);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    test_press_key(&game, allocator, '1');
    assert_test(game.selected_skill == 0);
    assert_test(game.mode == GAME_MODE_NONE);

    game_deinit(allocator, game);
}

PRIVATE void test_game_key_down_mode_none_noops(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    skill_list_add(allocator, &skills, SKILL_RANGED);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    // No tile clicked yet -> mode is still NONE, no buttons visible.
    test_press_key(&game, allocator, '1');
    assert_test(game.selected_skill == 0);
    assert_test(game.mode == GAME_MODE_NONE);

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
    { TEST_NAME("game_tile_pressed_enemy_active_noops"), test_game_tile_pressed_enemy_active_noops },
    { TEST_NAME("game_tile_pressed_mode_none_noops"), test_game_tile_pressed_mode_none_noops },
    { TEST_NAME("game_attack_toggle_enemy_active_noops"), test_game_attack_toggle_enemy_active_noops },
    { TEST_NAME("game_attack_toggle_mode_none_noops"), test_game_attack_toggle_mode_none_noops },
    { TEST_NAME("game_end_turn_enemy_active_noops"), test_game_end_turn_enemy_active_noops },
    { TEST_NAME("game_mouse_move_updates_hover"), test_game_mouse_move_updates_hover },
    { TEST_NAME("game_skill_button_hit_test_clamps_more_than_two_skills"), test_game_skill_button_hit_test_clamps_more_than_two_skills },
    { TEST_NAME("game_key_down_selects_visible_skill"), test_game_key_down_selects_visible_skill },
    { TEST_NAME("game_key_down_out_of_range_digit_noops"), test_game_key_down_out_of_range_digit_noops },
    { TEST_NAME("game_key_down_enemy_active_noops"), test_game_key_down_enemy_active_noops },
    { TEST_NAME("game_key_down_mode_none_noops"), test_game_key_down_mode_none_noops },
};

const uint32_t g_game_selection_tests_count = sizeof(g_game_selection_tests) / sizeof(g_game_selection_tests[0]);
