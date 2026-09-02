#include "test_game_ai.h"
#include "game/game.h"
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

PRIVATE void test_game_ai_adjacent_enemy_attacks_without_moving_on_end_turn(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *enemy_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    enemy->skills = (slice_skill_t){ .begin = enemy_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, enemy);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_end_turn(&game, allocator);

    assert_test(turn_active_entity(game.turn) == p);
    assert_test(enemy->position.x == 1);
    assert_test(enemy->position.y == 0);
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
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){4, 0}, 10, 2, 4);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *enemy_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    enemy->skills = (slice_skill_t){ .begin = enemy_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, enemy);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_end_turn(&game, allocator);

    assert_test(turn_active_entity(game.turn) == p);
    assert_test(enemy->position.x == 1);
    assert_test(enemy->position.y == 0);
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
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){4, 0}, 10, 2, 1);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *enemy_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    enemy->skills = (slice_skill_t){ .begin = enemy_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, enemy);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_end_turn(&game, allocator);

    assert_test(turn_active_entity(game.turn) == p);
    assert_test(enemy->position.x == 3);
    assert_test(enemy->position.y == 0);
    assert_test(enemy->mp == 0);
    assert_test(enemy->ap == 2);
    assert_test(!skill_can_target(grid, entities, enemy, SLICE_AT(enemy->skills, 0), p));
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
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){4, 1}, 10, 2, 3);
    entity_t* enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){0, 1}, 10, 2, 8);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *enemy_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    enemy->skills = (slice_skill_t){ .begin = enemy_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, enemy);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_end_turn(&game, allocator);

    assert_test(turn_active_entity(game.turn) == p);
    assert_test(skill_can_target(grid, entities, enemy, SLICE_AT(enemy->skills, 0), p));
    assert_test(p->hp == 5);
    assert_test(enemy->ap == 1);

    game_deinit(allocator, game);
}

PRIVATE void test_game_ai_multiple_enemies_act_independently_on_end_turn(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 30, 2, 3);
    entity_t* enemy_a = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 0}, 10, 2, 3);
    entity_t* enemy_b = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){0, 3}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *enemy_a_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    enemy_a->skills = (slice_skill_t){ .begin = enemy_a_skills_begin, .end = skills.end };
    skill_t *enemy_b_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    enemy_b->skills = (slice_skill_t){ .begin = enemy_b_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, enemy_a);
    turn_order_add(allocator, &order, enemy_b);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    // One end-turn plays both enemies' turns, then wraps back to p.
    test_click_end_turn(&game, allocator);

    assert_test(turn_active_entity(game.turn) == p);

    assert_test(enemy_a->position.x == 1);
    assert_test(enemy_a->position.y == 0);
    assert_test(enemy_a->mp == 1);
    assert_test(enemy_a->ap == 1);

    assert_test(enemy_b->position.x == 0);
    assert_test(enemy_b->position.y == 1);
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
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 3}, 10, 2, 0);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *enemy_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    enemy->skills = (slice_skill_t){ .begin = enemy_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, enemy);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_end_turn(&game, allocator);

    assert_test(turn_active_entity(game.turn) == p);
    assert_test(enemy->position.x == 3);
    assert_test(enemy->position.y == 3);
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
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 0}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *enemy_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_RANGED);
    enemy->skills = (slice_skill_t){ .begin = enemy_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, enemy);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_end_turn(&game, allocator);

    assert_test(turn_active_entity(game.turn) == p);
    // Already within SKILL_RANGED.range (3): no need to move in.
    assert_test(enemy->position.x == 3);
    assert_test(enemy->position.y == 0);
    assert_test(enemy->mp == 3);
    assert_test(enemy->ap == 1);
    assert_test(p->hp == 7);
    assert_test(p->alive);

    game_deinit(allocator, game);
}

// A multi-skill enemy prefers its highest-damage skill (melee > ranged) and
// closes distance for it when reachable, rather than settling for ranged
// just because it's already in range. Skills are added ranged-then-melee
// (weaker first) so the test only passes if ai_preferred_skill picks by
// damage, not list order.
PRIVATE void test_game_ai_multi_skill_enemy_closes_to_melee_range_when_reachable(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    // Distance 3: within SKILL_RANGED.range, outside SKILL_MELEE.range,
    // with enough mp (3) to close all the way in.
    entity_t* enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 0}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *enemy_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_RANGED);
    skill_list_add(allocator, &skills, SKILL_MELEE);
    enemy->skills = (slice_skill_t){ .begin = enemy_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, enemy);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_end_turn(&game, allocator);

    assert_test(turn_active_entity(game.turn) == p);
    // Closed to adjacency instead of stopping where ranged was usable.
    assert_test(enemy->position.x == 1);
    assert_test(enemy->position.y == 0);
    assert_test(enemy->mp == 1);
    assert_test(enemy->ap == 1);
    // SKILL_MELEE damage, not SKILL_RANGED: attacked with the preferred
    // (higher-damage) skill once in range.
    assert_test(p->hp == 10 - SKILL_MELEE.damage);
    assert_test(p->alive);

    game_deinit(allocator, game);
}

// Same setup, but not enough mp to reach melee -- falls back to ranged
// rather than attacking not at all.
PRIVATE void test_game_ai_multi_skill_enemy_falls_back_to_ranged_when_melee_unreachable(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    // Distance 3, mp=1 only closes to distance 2: still outside melee
    // range, still within ranged range.
    entity_t* enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 0}, 10, 2, 1);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *enemy_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    skill_list_add(allocator, &skills, SKILL_RANGED);
    enemy->skills = (slice_skill_t){ .begin = enemy_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, enemy);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_end_turn(&game, allocator);

    assert_test(turn_active_entity(game.turn) == p);
    assert_test(enemy->position.x == 2);
    assert_test(enemy->position.y == 0);
    assert_test(enemy->mp == 0);
    assert_test(enemy->ap == 1);
    // SKILL_RANGED.damage: melee was unreachable, so the fallback skill
    // was used instead.
    assert_test(p->hp == 10 - SKILL_RANGED.damage);
    assert_test(p->alive);

    game_deinit(allocator, game);
}

PRIVATE void test_game_ai_unreachable_player_noops_on_end_turn(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 5, 5);
    // p's only two neighbors are unwalkable, so no path reaches a tile
    // adjacent to p.
    grid_set_walkable(grid, (position_t){1, 0}, false);
    grid_set_walkable(grid, (position_t){0, 1}, false);

    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){4, 4}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *enemy_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    enemy->skills = (slice_skill_t){ .begin = enemy_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, enemy);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_end_turn(&game, allocator);

    // No reachable target: AI turn is a no-op, control wraps back to p.
    assert_test(turn_active_entity(game.turn) == p);
    assert_test(enemy->position.x == 4);
    assert_test(enemy->position.y == 4);
    assert_test(enemy->ap == 2);
    assert_test(p->hp == 10);

    game_deinit(allocator, game);
}

PRIVATE void test_game_ai_chooses_nearest_player_on_end_turn(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    // Far player spawned (iterated) first, near player second:
    // ai_find_nearest_player must replace the far candidate. Different axes
    // so neither occludes the other in the BFS.
    entity_t* p_far = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 3}, 10, 2, 3);
    entity_t* p_near = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){2, 0}, 10, 2, 3);
    entity_t* enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){0, 0}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_far_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p_far->skills = (slice_skill_t){ .begin = p_far_skills_begin, .end = skills.end };
    skill_t *p_near_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p_near->skills = (slice_skill_t){ .begin = p_near_skills_begin, .end = skills.end };
    skill_t *enemy_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    enemy->skills = (slice_skill_t){ .begin = enemy_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p_far);
    turn_order_add(allocator, &order, enemy);
    turn_order_add(allocator, &order, p_near);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_end_turn(&game, allocator);

    // Enemy closed on the nearer player, leaving the far one untouched;
    // cursor advanced to the near player.
    assert_test(turn_active_entity(game.turn) == p_near);
    assert_test(enemy->position.x == 1);
    assert_test(enemy->position.y == 0);
    assert_test(enemy->ap == 1);
    assert_test(p_near->hp == 5);
    assert_test(p_far->hp == 10);

    game_deinit(allocator, game);
}

// Exercises two ai_find_nearest_player branches single-player tests can't
// reach: the !candidate->alive skip (a dead player stays in the entity
// list; turn_remove_dead_entities only compacts turn order), and
// dist < best_dist evaluating false (nearer player scanned first).
PRIVATE void test_game_ai_skips_dead_player_and_keeps_nearest_on_end_turn(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    // Doomed player spawned first so iteration sees it before the survivor,
    // exercising the !alive skip once dead.
    entity_t* p_doomed = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 1}, 5, 2, 3);
    entity_t* p_alive = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    // e_killer is adjacent to p_doomed, two steps from p_alive: attacks
    // p_doomed, making p_alive the farther candidate (dist < best_dist false).
    entity_t* e_killer = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 1}, 10, 2, 3);
    entity_t* e_other = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 3}, 10, 2, 0);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_doomed_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p_doomed->skills = (slice_skill_t){ .begin = p_doomed_skills_begin, .end = skills.end };
    skill_t *p_alive_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p_alive->skills = (slice_skill_t){ .begin = p_alive_skills_begin, .end = skills.end };
    skill_t *e_killer_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e_killer->skills = (slice_skill_t){ .begin = e_killer_skills_begin, .end = skills.end };
    skill_t *e_other_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e_other->skills = (slice_skill_t){ .begin = e_other_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p_alive);
    turn_order_add(allocator, &order, e_killer);
    turn_order_add(allocator, &order, p_doomed);
    turn_order_add(allocator, &order, e_other);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_end_turn(&game, allocator);

    // e_killer killed p_doomed; e_other skipped the dead player, had no
    // target (mp=0), so control wrapped to p_alive unharmed.
    assert_test(!p_doomed->alive);
    assert_test(p_alive->alive);
    assert_test(p_alive->hp == 10);
    assert_test(turn_active_entity(game.turn) == p_alive);

    game_deinit(allocator, game);
}

// Both skills already in range: ai_best_in_range_skill's "later skill beats
// current best?" comparison must evaluate false when earlier is stronger.
PRIVATE void test_game_ai_best_in_range_skill_rejects_weaker_later_skill(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 2, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    // Melee first, ranged second: both in range at adjacency, ranged must
    // be rejected as weaker.
    skill_t *enemy_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    skill_list_add(allocator, &skills, SKILL_RANGED);
    enemy->skills = (slice_skill_t){ .begin = enemy_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, enemy);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_end_turn(&game, allocator);

    // Stronger melee skill was used, not weaker ranged; enemy never moved.
    assert_test(turn_active_entity(game.turn) == p);
    assert_test(enemy->position.x == 1);
    assert_test(enemy->position.y == 0);
    assert_test(enemy->ap == 1);
    assert_test(p->hp == 10 - SKILL_MELEE.damage);
    assert_test(p->alive);

    game_deinit(allocator, game);
}

PRIVATE void test_game_ai_equal_damage_skills_prefer_lower_ap_cost(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 0}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };

    skill_t expensive = { .range = 1, .damage = 5, .ap_cost = 2 };
    skill_t cheap = { .range = 1, .damage = 5, .ap_cost = 1 };
    skill_t *enemy_skills_begin = skills.end;
    skill_list_add(allocator, &skills, expensive);
    skill_list_add(allocator, &skills, cheap);
    enemy->skills = (slice_skill_t){ .begin = enemy_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, enemy);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_end_turn(&game, allocator);

    // Equal damage, different ap_cost: the AI prefers the cheaper skill and
    // attacks with it once in range.
    assert_test(turn_active_entity(game.turn) == p);
    assert_test(enemy->position.x == 1);
    assert_test(enemy->position.y == 0);
    assert_test(enemy->ap == 1);
    assert_test(p->hp == 5);

    game_deinit(allocator, game);
}

PRIVATE void test_game_ai_attack_noops_when_ap_insufficient_for_skill(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 2, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 10, 1, 0);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };

    skill_t costly = { .range = 1, .damage = 5, .ap_cost = 2 };
    skill_t *enemy_skills_begin = skills.end;
    skill_list_add(allocator, &skills, costly);
    enemy->skills = (slice_skill_t){ .begin = enemy_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, enemy);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_end_turn(&game, allocator);

    // In range but costs more AP than the enemy has: action_try_attack
    // rejects it, no damage.
    assert_test(turn_active_entity(game.turn) == p);
    assert_test(enemy->position.x == 1);
    assert_test(enemy->position.y == 0);
    assert_test(enemy->ap == 1);
    assert_test(p->hp == 10);

    game_deinit(allocator, game);
}

// Enemy with only an AoE skill (SKILL_FIREBALL) casts on the target's tile,
// already in range so no movement.
PRIVATE void test_game_ai_aoe_enemy_hits_target_with_blast_on_end_turn(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 0}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *enemy_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_FIREBALL);
    enemy->skills = (slice_skill_t){ .begin = enemy_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, enemy);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_end_turn(&game, allocator);

    assert_test(turn_active_entity(game.turn) == p);
    // Already within SKILL_FIREBALL.range: no need to move in.
    assert_test(enemy->position.x == 3);
    assert_test(enemy->position.y == 0);
    assert_test(enemy->mp == 3);
    assert_test(enemy->ap == 1);
    assert_test(p->hp == 10 - SKILL_FIREBALL.damage);
    assert_test(p->alive);

    game_deinit(allocator, game);
}

// AoE version of test_game_ai_attack_noops_when_ap_insufficient_for_skill.
PRIVATE void test_game_ai_aoe_attack_noops_when_ap_insufficient_for_skill(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 2, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 10, 1, 0);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };

    skill_t costly_aoe = { .range = 4, .aoe_radius = 2, .damage = 4, .ap_cost = 2 };
    skill_t *enemy_skills_begin = skills.end;
    skill_list_add(allocator, &skills, costly_aoe);
    enemy->skills = (slice_skill_t){ .begin = enemy_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, enemy);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_end_turn(&game, allocator);

    assert_test(turn_active_entity(game.turn) == p);
    assert_test(enemy->position.x == 1);
    assert_test(enemy->position.y == 0);
    assert_test(enemy->ap == 1);
    assert_test(p->hp == 10);
    assert_test(p->alive);

    game_deinit(allocator, game);
}

// Blast damages a bystander without killing it: p1 dies, p2 survives --
// exercises the "hit but still alive" branch of dead-splicing.
PRIVATE void test_game_ai_aoe_blast_damages_bystander_without_killing_on_end_turn(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p1 = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){1, 0}, 4, 2, 3);
    entity_t* enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){0, 0}, 10, 2, 3);
    // In blast radius of p1's tile, but hp enough to survive.
    entity_t* p2 = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 1}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p1_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p1->skills = (slice_skill_t){ .begin = p1_skills_begin, .end = skills.end };
    skill_t *enemy_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_FIREBALL);
    enemy->skills = (slice_skill_t){ .begin = enemy_skills_begin, .end = skills.end };
    skill_t *p2_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p2->skills = (slice_skill_t){ .begin = p2_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p1);
    turn_order_add(allocator, &order, enemy);
    turn_order_add(allocator, &order, p2);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_end_turn(&game, allocator);

    assert_test(!p1->alive);
    assert_test(p2->alive);
    assert_test(p2->hp == 10 - SKILL_FIREBALL.damage);
    assert_test(enemy->ap == 2 - SKILL_FIREBALL.ap_cost);
    assert_test(game.game_over == GAME_OVER_NONE);
    assert_test(turn_active_entity(game.turn) == p2);

    game_deinit(allocator, game);
}

// Fireball on the nearest target also catches a second player in blast
// radius, killing both; a third outside the blast survives.
PRIVATE void test_game_ai_aoe_blast_kills_multiple_players_on_end_turn(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    // p1 spawned first, wins the ai_find_nearest_player tie vs p2.
    entity_t* p1 = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){1, 0}, 4, 2, 3);
    entity_t* enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){0, 0}, 10, 2, 3);
    // In blast radius of p1, but not the impact tile.
    entity_t* p2 = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 1}, 4, 2, 3);
    // Outside blast radius.
    entity_t* p3 = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){3, 3}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p1_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p1->skills = (slice_skill_t){ .begin = p1_skills_begin, .end = skills.end };
    skill_t *enemy_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_FIREBALL);
    enemy->skills = (slice_skill_t){ .begin = enemy_skills_begin, .end = skills.end };
    skill_t *p2_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p2->skills = (slice_skill_t){ .begin = p2_skills_begin, .end = skills.end };
    skill_t *p3_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p3->skills = (slice_skill_t){ .begin = p3_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p1);
    turn_order_add(allocator, &order, enemy);
    turn_order_add(allocator, &order, p2);
    turn_order_add(allocator, &order, p3);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_end_turn(&game, allocator);

    assert_test(!p1->alive);
    assert_test(!p2->alive);
    assert_test(p3->alive);
    assert_test(p3->hp == 10);
    assert_test(enemy->position.x == 0);
    assert_test(enemy->position.y == 0);
    assert_test(enemy->ap == 2 - SKILL_FIREBALL.ap_cost);
    assert_test(game.game_over == GAME_OVER_NONE);
    assert_test(turn_active_entity(game.turn) == p3);

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
    { TEST_NAME("game_ai_multi_skill_enemy_closes_to_melee_range_when_reachable"), test_game_ai_multi_skill_enemy_closes_to_melee_range_when_reachable },
    { TEST_NAME("game_ai_multi_skill_enemy_falls_back_to_ranged_when_melee_unreachable"), test_game_ai_multi_skill_enemy_falls_back_to_ranged_when_melee_unreachable },
    { TEST_NAME("game_ai_unreachable_player_noops_on_end_turn"), test_game_ai_unreachable_player_noops_on_end_turn },
    { TEST_NAME("game_ai_chooses_nearest_player_on_end_turn"), test_game_ai_chooses_nearest_player_on_end_turn },
    { TEST_NAME("game_ai_skips_dead_player_and_keeps_nearest_on_end_turn"), test_game_ai_skips_dead_player_and_keeps_nearest_on_end_turn },
    { TEST_NAME("game_ai_best_in_range_skill_rejects_weaker_later_skill"), test_game_ai_best_in_range_skill_rejects_weaker_later_skill },
    { TEST_NAME("game_ai_equal_damage_skills_prefer_lower_ap_cost"), test_game_ai_equal_damage_skills_prefer_lower_ap_cost },
    { TEST_NAME("game_ai_attack_noops_when_ap_insufficient_for_skill"), test_game_ai_attack_noops_when_ap_insufficient_for_skill },
    { TEST_NAME("game_ai_aoe_enemy_hits_target_with_blast_on_end_turn"), test_game_ai_aoe_enemy_hits_target_with_blast_on_end_turn },
    { TEST_NAME("game_ai_aoe_attack_noops_when_ap_insufficient_for_skill"), test_game_ai_aoe_attack_noops_when_ap_insufficient_for_skill },
    { TEST_NAME("game_ai_aoe_blast_damages_bystander_without_killing_on_end_turn"), test_game_ai_aoe_blast_damages_bystander_without_killing_on_end_turn },
    { TEST_NAME("game_ai_aoe_blast_kills_multiple_players_on_end_turn"), test_game_ai_aoe_blast_kills_multiple_players_on_end_turn },
};

const uint32_t g_game_ai_tests_count = sizeof(g_game_ai_tests) / sizeof(g_game_ai_tests[0]);
