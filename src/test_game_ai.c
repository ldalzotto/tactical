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

    // One end-turn press plays out both enemies' turns in order, back
    // around to the player.
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

// A multi-skill enemy prefers its highest-damage skill (melee,
// SKILL_MELEE.damage=5 > SKILL_RANGED.damage=3) and closes distance for it
// when it has enough mp to reach melee range this turn, rather than settling
// for ranged just because it's already in range.
// Enemy's skills are added SKILL_RANGED then SKILL_MELEE, deliberately so
// skills[0] is the WEAKER skill -- this way the test only passes if
// ai_preferred_skill actually picks by damage, not by coincidentally
// matching whichever skill happens to be first in the list.
PRIVATE void test_game_ai_multi_skill_enemy_closes_to_melee_range_when_reachable(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    // Starts at distance 3: already within SKILL_RANGED.range (3), outside
    // SKILL_MELEE.range (1), with enough mp (3) to close all the way in.
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
    // Closed all the way to adjacency (distance 1) instead of stopping at
    // distance 2 or 3 where ranged was already usable.
    assert_test(enemy->position.x == 1);
    assert_test(enemy->position.y == 0);
    assert_test(enemy->mp == 1);
    assert_test(enemy->ap == 1);
    // SKILL_MELEE.damage (5), not SKILL_RANGED.damage (3): attacked with the
    // preferred (higher-damage) skill, since it ended up in range.
    assert_test(p->hp == 10 - SKILL_MELEE.damage);
    assert_test(p->alive);

    game_deinit(allocator, game);
}

// Same setup, but not enough mp to reach melee range this turn -- falls back
// to ranged (still in range) rather than landing no attack at all.
PRIVATE void test_game_ai_multi_skill_enemy_falls_back_to_ranged_when_melee_unreachable(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    // Starts at distance 3, but mp=1 only closes to distance 2 -- still
    // outside SKILL_MELEE.range (1), still within SKILL_RANGED.range (3).
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
    // SKILL_RANGED.damage (3): melee was never reachable this turn, so the
    // fallback (still-in-range) skill was used instead of no attack at all.
    assert_test(p->hp == 10 - SKILL_RANGED.damage);
    assert_test(p->alive);

    game_deinit(allocator, game);
}

PRIVATE void test_game_ai_unreachable_player_noops_on_end_turn(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 5, 5);
    // p is boxed into the top-left corner: its only two neighbors are
    // unwalkable, so no path from the enemy reaches a tile adjacent to p.
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

    // No reachable player target: the AI turn is a no-op and control wraps
    // back to p.
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
    // Far player is spawned (and therefore iterated) first; the near player
    // second. ai_find_nearest_player must replace the far candidate when it
    // reaches the nearer one. The two players are on different axes so the
    // nearer one doesn't occlude the far one in the BFS.
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

    // Enemy closed on the nearer player and attacked it, leaving the far one
    // untouched. The cursor then advanced to the near player.
    assert_test(turn_active_entity(game.turn) == p_near);
    assert_test(enemy->position.x == 1);
    assert_test(enemy->position.y == 0);
    assert_test(enemy->ap == 1);
    assert_test(p_near->hp == 5);
    assert_test(p_far->hp == 10);

    game_deinit(allocator, game);
}

// Exercises two AI candidate-selection branches that passing end-turn play
// can reach but the simpler single-player tests don't:
// - ai_find_nearest_player's !candidate->alive skip (a dead player stays in
//   the entity list after turn_remove_dead_entities compacts only the turn
//   order).
// - ai_find_nearest_player's dist < best_dist comparison evaluated false
//   (a nearer player is scanned before a farther one).
PRIVATE void test_game_ai_skips_dead_player_and_keeps_nearest_on_end_turn(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    // Spawn the doomed player first so entity iteration sees it before the
    // surviving player -- once dead, it exercises the !alive skip on the
    // next enemy turn.
    entity_t* p_doomed = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 1}, 5, 2, 3);
    entity_t* p_alive = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    // e_killer starts adjacent to p_doomed (distance 1) and two steps from
    // p_alive, so it attacks p_doomed. That makes p_alive the "farther
    // candidate" whose dist < best_dist is false in the same scan.
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

    // e_killer killed p_doomed; e_other then skipped the dead player and
    // had no target in range (mp=0), so control wrapped to p_alive unharmed.
    assert_test(!p_doomed->alive);
    assert_test(p_alive->alive);
    assert_test(p_alive->hp == 10);
    assert_test(turn_active_entity(game.turn) == p_alive);

    game_deinit(allocator, game);
}

// A multi-skill enemy with both skills already in range must still evaluate
// ai_best_in_range_skill's "does this later skill beat the current best?"
// comparison as false when the earlier skill is stronger.
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
    // Melee (damage 5) first, ranged (damage 3) second: both are in range
    // at adjacency, and the later ranged skill must be rejected as weaker.
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

    // The stronger melee skill (damage 5) was used, not the weaker ranged
    // fallback, and the enemy never needed to move.
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

    // The only skill is in range but costs more AP than the enemy has, so
    // action_try_attack rejects it and the AI turn ends without damage.
    assert_test(turn_active_entity(game.turn) == p);
    assert_test(enemy->position.x == 1);
    assert_test(enemy->position.y == 0);
    assert_test(enemy->ap == 1);
    assert_test(p->hp == 10);

    game_deinit(allocator, game);
}

// An enemy with only an AoE skill (SKILL_FIREBALL) casts it on the target's
// tile -- already in range, so no movement.
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
    // Already within SKILL_FIREBALL.range (4): no need to move in.
    assert_test(enemy->position.x == 3);
    assert_test(enemy->position.y == 0);
    assert_test(enemy->mp == 3);
    assert_test(enemy->ap == 1);
    assert_test(p->hp == 10 - SKILL_FIREBALL.damage);
    assert_test(p->alive);

    game_deinit(allocator, game);
}

// AoE version of test_game_ai_attack_noops_when_ap_insufficient_for_skill:
// the in-range skill costs more AP than the enemy has, so the cast is rejected.
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

// Blast damages a bystander without killing it: p1 (the target) dies, p2
// survives -- exercises the "hit but still alive" branch of dead-splicing.
PRIVATE void test_game_ai_aoe_blast_damages_bystander_without_killing_on_end_turn(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p1 = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){1, 0}, 4, 2, 3);
    entity_t* enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){0, 0}, 10, 2, 3);
    // In blast radius (2) of p1's tile but has enough hp to survive it.
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

// A fireball on the nearest target also catches a second player within
// blast radius, killing both in one cast; a third player outside the blast
// survives untouched.
PRIVATE void test_game_ai_aoe_blast_kills_multiple_players_on_end_turn(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    // p1 spawned first, so it wins the ai_find_nearest_player tie against p2.
    entity_t* p1 = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){1, 0}, 4, 2, 3);
    entity_t* enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){0, 0}, 10, 2, 3);
    // In blast radius (2) of p1 but not itself the impact tile.
    entity_t* p2 = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 1}, 4, 2, 3);
    // Outside the blast radius, so untouched.
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
