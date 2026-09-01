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
    // hp bumped to 20: with ap_cost 1 skills and the enemy's default 2 ap,
    // a multi-action turn lands two melee hits (see ai_run_ennemy_turn) --
    // 10 hp would die outright, which isn't what this test is about.
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 20, 2, 3);
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
    // Multi-action turn: with 2 ap and a 1-ap-cost melee skill, the enemy
    // attacks twice (see ai_run_ennemy_turn) before running out of ap.
    assert_test(enemy->ap == 0);
    assert_test(p->hp == 20 - 2 * SKILL_MELEE.damage);
    assert_test(p->alive);

    game_deinit(allocator, game);
}

PRIVATE void test_game_ai_far_enemy_with_enough_mp_closes_and_attacks_on_end_turn(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 5, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    // See the ap-0/hp-20 comment on the previous test: two ap-cost-1 melee
    // hits land once the enemy is in range.
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 20, 2, 3);
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
    assert_test(enemy->ap == 0);
    assert_test(p->hp == 20 - 2 * SKILL_MELEE.damage);
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
    // hp bumped to 20 -- see the ap-0 comment below.
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){4, 1}, 20, 2, 3);
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
    // Multi-action turn: two ap-cost-1 melee hits once the detour lands the
    // enemy in range, exhausting its 2 ap.
    assert_test(p->hp == 20 - 2 * SKILL_MELEE.damage);
    assert_test(enemy->ap == 0);

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
    // Multi-action turn: each enemy lands two ap-cost-1 melee hits once in
    // range, exhausting its 2 ap.
    assert_test(enemy_a->ap == 0);

    assert_test(enemy_b->position.x == 0);
    assert_test(enemy_b->position.y == 1);
    assert_test(enemy_b->mp == 1);
    assert_test(enemy_b->ap == 0);

    // 4 hits total (2 from each enemy) at SKILL_MELEE.damage each.
    assert_test(p->hp == 30 - 4 * SKILL_MELEE.damage);
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
    // Multi-action turn: two ap-cost-1 ranged hits, exhausting the enemy's
    // 2 ap without ever needing to move.
    assert_test(enemy->ap == 0);
    assert_test(p->hp == 10 - 2 * SKILL_RANGED.damage);
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
    // hp bumped to 20 -- see the ap-0 comment below.
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 20, 2, 3);
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
    // Multi-action turn: two ap-cost-1 melee hits once in range, exhausting
    // the enemy's 2 ap.
    assert_test(enemy->ap == 0);
    // SKILL_MELEE.damage (5), not SKILL_RANGED.damage (3): attacked with the
    // preferred (higher-damage) skill, since it ended up in range.
    assert_test(p->hp == 20 - 2 * SKILL_MELEE.damage);
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
    // Multi-action turn: melee never comes into range this turn (mp runs
    // out), so both of the enemy's 2 ap go into the fallback ranged skill.
    assert_test(enemy->ap == 0);
    // SKILL_RANGED.damage (3) x2: melee was never reachable this turn, so
    // the fallback (still-in-range) skill was used for both attacks.
    assert_test(p->hp == 10 - 2 * SKILL_RANGED.damage);
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
    // second. Both start at equal hp/threat, so ai_choose_best_target's
    // distance term must still prefer the closer one when it reaches it.
    // The two players are on different axes so the nearer one doesn't
    // occlude the far one in the BFS. p_near's hp is bumped to 20 -- see the
    // ap-0 comment below.
    entity_t* p_far = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 3}, 10, 2, 3);
    entity_t* p_near = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){2, 0}, 20, 2, 3);
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
    // Multi-action turn: two ap-cost-1 melee hits once in range, exhausting
    // the enemy's 2 ap.
    assert_test(enemy->ap == 0);
    assert_test(p_near->hp == 20 - 2 * SKILL_MELEE.damage);
    assert_test(p_far->hp == 10);

    game_deinit(allocator, game);
}

// Exercises two AI candidate-selection branches that passing end-turn play
// can reach but the simpler single-player tests don't:
// - ai_choose_best_target's !candidate->alive skip (a dead player stays in
//   the entity list after turn_remove_dead_entities compacts only the turn
//   order).
// - ai_choose_best_target's score > best_score comparison evaluated false
//   (a nearer player is scanned before a farther one).
// It also exercises a multi-action turn switching targets mid-turn:
// e_killer kills p_doomed with its first attack, then spends its remaining
// ap closing on and attacking p_alive instead of stopping there.
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

    // e_killer killed p_doomed with its first attack, then used its
    // remaining ap to close on and hit p_alive once. e_other then skipped
    // the dead player and had no target in range (mp=0), so control wrapped
    // to p_alive.
    assert_test(!p_doomed->alive);
    assert_test(p_alive->alive);
    assert_test(p_alive->hp == 10 - SKILL_MELEE.damage);
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
    // hp bumped to 20 -- see the ap-0 comment below.
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 20, 2, 3);
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
    // fallback, and the enemy never needed to move. Multi-action turn: two
    // such hits land, exhausting the enemy's 2 ap.
    assert_test(turn_active_entity(game.turn) == p);
    assert_test(enemy->position.x == 1);
    assert_test(enemy->position.y == 0);
    assert_test(enemy->ap == 0);
    assert_test(p->hp == 20 - 2 * SKILL_MELEE.damage);
    assert_test(p->alive);

    game_deinit(allocator, game);
}

PRIVATE void test_game_ai_equal_damage_skills_prefer_lower_ap_cost(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    // hp bumped to 20 -- see the ap-0 comment below.
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 20, 2, 3);
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
    // attacks with it once in range. Multi-action turn: the cheap skill's
    // 1 ap cost affords two hits from the enemy's 2 ap.
    assert_test(turn_active_entity(game.turn) == p);
    assert_test(enemy->position.x == 1);
    assert_test(enemy->position.y == 0);
    assert_test(enemy->ap == 0);
    assert_test(p->hp == 20 - 2 * cheap.damage);

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
    // Multi-action turn: two ap-cost-1 casts, exhausting the enemy's 2 ap.
    assert_test(enemy->ap == 0);
    assert_test(p->hp == 10 - 2 * SKILL_FIREBALL.damage);
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
// It's also a multi-action turn: p1's death doesn't end the enemy's turn --
// its remaining ap goes into a second cast, this time centered on p2 (the
// only player left).
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
    // Two casts land: the first (centered on p1) also catches p2 in the
    // blast; the second, cast after p1 dies, centers on p2 directly.
    assert_test(p2->hp == 10 - 2 * SKILL_FIREBALL.damage);
    assert_test(enemy->ap == 2 - 2 * SKILL_FIREBALL.ap_cost);
    assert_test(game.game_over == GAME_OVER_NONE);
    assert_test(turn_active_entity(game.turn) == p2);

    game_deinit(allocator, game);
}

// A fireball on the nearest target also catches a second player within
// blast radius, killing both in one cast. That's a multi-action turn too:
// with both p1 and p2 dead and ap still left, the enemy closes on and casts
// a second fireball on p3 (the only player left), so p3 no longer comes out
// fully untouched -- just outside the first cast's blast.
PRIVATE void test_game_ai_aoe_blast_kills_multiple_players_on_end_turn(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    // p1 spawned first, so it wins the ai_choose_best_target tie against p2.
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
    // The enemy closes 2 steps toward p3 (walking distance is exact in this
    // open grid regardless of which tie-broken path ai_step_toward takes),
    // landing it exactly at SKILL_FIREBALL.range (4) -- close enough for one
    // more cast, not close enough to move again this turn.
    assert_test(enemy->mp == 3 - 2);
    assert_test(skill_can_target(grid, entities, enemy, SLICE_AT(enemy->skills, 0), p3));
    assert_test(p3->hp == 10 - SKILL_FIREBALL.damage);
    assert_test(enemy->ap == 2 - 2 * SKILL_FIREBALL.ap_cost);
    assert_test(game.game_over == GAME_OVER_NONE);
    assert_test(turn_active_entity(game.turn) == p3);

    game_deinit(allocator, game);
}

// Target prioritization: a farther, killable player is chosen over a
// nearer, healthy one, and the enemy's second action (still within its 2
// ap) then switches to the survivor rather than stopping after the kill.
// p_far and p_near share identical stats otherwise, so this isolates the
// lethal-hit scoring bonus (see ai_score_target) from hp/threat factors.
PRIVATE void test_game_ai_prioritizes_killable_target_over_nearer_healthy_one_on_end_turn(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 2);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    // Adjacent to the enemy (dist-to-adjacency 0), full hp: not killable by
    // a single SKILL_RANGED hit (damage 3 < hp 10). Off the enemy-to-p_far
    // row so it doesn't block line of sight to p_far (skill_can_target).
    entity_t* p_near = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 1}, 10, 2, 3);
    entity_t* enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){0, 0}, 10, 2, 0);
    // 2 steps farther (dist-to-adjacency 2), but at exactly SKILL_RANGED's
    // damage: killable in one hit, still within range (3) from the enemy's
    // starting tile, so the enemy never needs to move for either target.
    entity_t* p_far = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){3, 0}, 3, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_near_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p_near->skills = (slice_skill_t){ .begin = p_near_skills_begin, .end = skills.end };
    skill_t *enemy_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_RANGED);
    enemy->skills = (slice_skill_t){ .begin = enemy_skills_begin, .end = skills.end };
    skill_t *p_far_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p_far->skills = (slice_skill_t){ .begin = p_far_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p_near);
    turn_order_add(allocator, &order, enemy);
    turn_order_add(allocator, &order, p_far);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_end_turn(&game, allocator);

    // First action: the lethal bonus outweighs p_near's distance advantage,
    // so p_far (farther but killable) is targeted and dies.
    assert_test(!p_far->alive);
    // Second action: p_far is gone, so the enemy's remaining ap goes into
    // p_near instead of stopping after one kill.
    assert_test(p_near->alive);
    assert_test(p_near->hp == 10 - SKILL_RANGED.damage);
    assert_test(enemy->ap == 0);
    assert_test(turn_active_entity(game.turn) == p_near);

    game_deinit(allocator, game);
}

// Self-preservation: a badly wounded enemy backs away from a player who
// threatens to kill it, using its full mp across successive actions, rather
// than trading a chip-damage hit for the risk of dying next turn.
PRIVATE void test_game_ai_retreats_when_badly_wounded_on_end_turn(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 5, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){2, 0}, 20, 2, 2);
    // Below AI_RETREAT_HP_PERCENT (25%) of its own max_hp (20) -- entity_spawn
    // has no separate max_hp parameter (max_hp always equals the spawned
    // hp, see entity_spawn), so wound it down directly after spawning.
    enemy->hp = 4;

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    // p's melee damage (5) exceeds the enemy's current hp (4): a real
    // threat, which is what triggers the retreat (see ai_should_retreat).
    // The second, weaker skill (SKILL_RANGED, damage 3) exercises
    // ai_entity_threat's "a later skill isn't the new max" branch.
    skill_list_add(allocator, &skills, SKILL_MELEE);
    skill_list_add(allocator, &skills, SKILL_RANGED);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *enemy_skills_begin = skills.end;
    // In range (3) from the start, but its damage (3) can't one-shot p (hp
    // 10): not lethal, so it doesn't override the retreat.
    skill_list_add(allocator, &skills, SKILL_RANGED);
    enemy->skills = (slice_skill_t){ .begin = enemy_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, enemy);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_end_turn(&game, allocator);

    // Retreated the full 2 mp (to x=4, the farthest reachable tile from p)
    // instead of attacking -- no ap spent, p untouched.
    assert_test(turn_active_entity(game.turn) == p);
    assert_test(enemy->position.x == 4);
    assert_test(enemy->position.y == 0);
    assert_test(enemy->mp == 0);
    assert_test(enemy->ap == 2);
    assert_test(enemy->hp == 4);
    assert_test(p->hp == 10);
    assert_test(p->alive);

    game_deinit(allocator, game);
}

// Retreat direction picking: an enemy backed into a corner where "up" and
// "right" tie on distance from the target must still find "down" -- checked
// later, in POSITION_DIRECTIONS order -- once it's strictly farther, and
// must skip an unwalkable neighbor ("left") instead of treating it as a
// candidate. Covers ai_step_away's later-tile-wins update and its
// unwalkable-neighbor skip, plus the ternary's negative-dx path.
PRIVATE void test_game_ai_retreat_prefers_farther_tile_and_skips_unwalkable_neighbor_on_end_turn(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);
    grid_set_walkable(grid, (position_t){0, 2}, false);

    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){3, 0}, 10, 2, 3);
    entity_t* enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 2}, 20, 2, 2);
    // Below AI_RETREAT_HP_PERCENT, same as the other retreat tests.
    enemy->hp = 4;

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    // Threatens the enemy (damage 5 >= its 4 hp) from any range -- it never
    // needs to be in range itself for ai_should_retreat's threat check.
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *enemy_skills_begin = skills.end;
    // Melee range (1) is never in range of p at this distance, so
    // ai_should_retreat's in-range/lethal exemption never applies -- simpler
    // than balancing a non-lethal in-range hit like the other retreat tests.
    skill_list_add(allocator, &skills, SKILL_MELEE);
    enemy->skills = (slice_skill_t){ .begin = enemy_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, enemy);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_end_turn(&game, allocator);

    // Action 1 from (1,2): up (1,1) and right (2,2) both score manhattan 3
    // from p (3,0); up is found first and stands unbeaten until down (1,3)
    // scores 5, strictly farther; left (0,2) is unwalkable and skipped.
    // Action 2 from (1,3): up (1,2) and right (2,3) tie at manhattan 4;
    // left (0,3) scores 6 and wins. mp (2) is spent after these two moves,
    // so a third action retreats no further and never attacks.
    assert_test(turn_active_entity(game.turn) == p);
    assert_test(enemy->position.x == 0);
    assert_test(enemy->position.y == 3);
    assert_test(enemy->mp == 0);
    assert_test(enemy->ap == 2);
    assert_test(enemy->hp == 4);
    assert_test(p->hp == 10);
    assert_test(p->alive);

    game_deinit(allocator, game);
}

// Retreat direction picking, vertical: covers the ternary's negative-dy
// path (test_game_ai_retreat_prefers_farther_tile_and_skips_unwalkable_neighbor_on_end_turn
// only ever exercises negative dx, since its target sits in the enemy's
// bottom row).
PRIVATE void test_game_ai_retreats_vertically_away_from_target_on_end_turn(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 2, 6);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 4}, 10, 2, 3);
    entity_t* enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){0, 2}, 20, 2, 2);
    enemy->hp = 4;

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

    // "up" wins over "right"/"down" every action (both mp), retreating from
    // y=2 to y=0 -- each step's neighbor has y < p's y=4, i.e. negative dy.
    assert_test(turn_active_entity(game.turn) == p);
    assert_test(enemy->position.x == 0);
    assert_test(enemy->position.y == 0);
    assert_test(enemy->mp == 0);
    assert_test(enemy->ap == 2);
    assert_test(enemy->hp == 4);
    assert_test(p->hp == 10);
    assert_test(p->alive);

    game_deinit(allocator, game);
}

// A retreating enemy fully boxed in (every neighbor either the adjacent
// target's own tile or unwalkable) finds nowhere to go: ai_step_away's
// found stays false, so it no-ops instead of moving.
PRIVATE void test_game_ai_retreat_noops_when_fully_boxed_in_on_end_turn(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 3, 3);
    grid_set_walkable(grid, (position_t){2, 1}, false);
    grid_set_walkable(grid, (position_t){1, 2}, false);
    grid_set_walkable(grid, (position_t){0, 1}, false);

    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    // p occupies the enemy's one remaining (non-walled) neighbor.
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){1, 0}, 10, 2, 3);
    entity_t* enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 1}, 20, 2, 2);
    enemy->hp = 4;

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *enemy_skills_begin = skills.end;
    // In range (3) but not lethal (damage 3 < p's 10 hp), so it doesn't
    // exempt the enemy from retreating -- same shape as the other retreat
    // tests, just with the retreat itself going nowhere.
    skill_list_add(allocator, &skills, SKILL_RANGED);
    enemy->skills = (slice_skill_t){ .begin = enemy_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, enemy);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_end_turn(&game, allocator);

    // No reachable neighbor to retreat to, and the retreat decision (not a
    // lethal shot) means no attack either: the turn is a full no-op.
    assert_test(turn_active_entity(game.turn) == p);
    assert_test(enemy->position.x == 1);
    assert_test(enemy->position.y == 1);
    assert_test(enemy->mp == 2);
    assert_test(enemy->ap == 2);
    assert_test(enemy->hp == 4);
    assert_test(p->hp == 10);
    assert_test(p->alive);

    game_deinit(allocator, game);
}

// A badly wounded enemy with a lethal shot already lined up takes the kill
// instead of retreating -- covers ai_should_retreat's in-range-and-lethal
// exemption actually firing true.
PRIVATE void test_game_ai_takes_lethal_shot_instead_of_retreating_on_end_turn(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 2, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    // Killable in one melee hit (damage 5 >= hp 3).
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 3, 2, 3);
    entity_t* enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 20, 2, 3);
    // Below AI_RETREAT_HP_PERCENT, and p's melee (damage 5) would threaten
    // it too -- would retreat if not for the lethal shot already in range.
    enemy->hp = 4;

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

    // Kills p instead of retreating; p was the only player, so the game
    // ends. With p removed from the turn order, the trailing turn_advance
    // in game_advance_turn wraps the cursor back to enemy (the only entity
    // left) -- and turn_advance resets whoever becomes active's ap/mp to
    // max, even when that's the same entity, so ap/mp read back at their
    // spawn values despite the attack having spent 1 ap.
    assert_test(!p->alive);
    assert_test(enemy->position.x == 1);
    assert_test(enemy->position.y == 0);
    assert_test(enemy->mp == 3);
    assert_test(enemy->ap == 2);
    assert_test(enemy->hp == 4);
    assert_test(game.game_over == GAME_OVER_LOSE);
    assert_test(turn_active_entity(game.turn) == enemy);

    game_deinit(allocator, game);
}

// A multi-action turn where an action moves into range but then can't
// afford the attack: covers ai_run_ennemy_action's final progress check
// being satisfied by mp alone (movement happened) while ap didn't change
// (the attack failed) -- the two are checked separately in the retreat
// branch already, but this is the only place the attack-path return can
// see mp change without ap changing in the same action.
PRIVATE void test_game_ai_multi_action_turn_after_moving_into_range_with_insufficient_ap_on_end_turn(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 3, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){2, 0}, 10, 1, 2);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };

    // Costs more ap (2) than the enemy has (1): closing into range never
    // pays off this turn.
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

    // Closes to adjacency (mp 2 -> 1) and tries to attack, but 1 ap can't
    // cover the 2-ap skill: the attack fails, ap unchanged, no damage.
    assert_test(turn_active_entity(game.turn) == p);
    assert_test(enemy->position.x == 1);
    assert_test(enemy->position.y == 0);
    assert_test(enemy->mp == 1);
    assert_test(enemy->ap == 1);
    assert_test(p->hp == 10);
    assert_test(p->alive);

    game_deinit(allocator, game);
}

// An enemy with far more ap than it needs per hit keeps attacking until it
// exhausts AI_MAX_ACTIONS_PER_TURN's full budget (8 actions here, one per
// ap) -- covers ai_run_ennemy_turn's loop ending via its own bound instead
// of an internal break.
PRIVATE void test_game_ai_multi_action_turn_runs_full_action_budget_on_end_turn(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 2, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    // Enough hp to survive all 8 hits (8 damage) many times over.
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 100, 2, 3);
    entity_t* enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 10, 8, 0);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };

    skill_t weak = { .range = 1, .damage = 1, .ap_cost = 1 };
    skill_t *enemy_skills_begin = skills.end;
    skill_list_add(allocator, &skills, weak);
    enemy->skills = (slice_skill_t){ .begin = enemy_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, enemy);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_end_turn(&game, allocator);

    // 8 ap at 1 ap/hit lands exactly 8 hits, exhausting ap on the same
    // action that hits AI_MAX_ACTIONS_PER_TURN's bound.
    assert_test(turn_active_entity(game.turn) == p);
    assert_test(enemy->position.x == 1);
    assert_test(enemy->position.y == 0);
    assert_test(enemy->ap == 0);
    assert_test(p->hp == 100 - 8 * weak.damage);
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
    { TEST_NAME("game_ai_prioritizes_killable_target_over_nearer_healthy_one_on_end_turn"), test_game_ai_prioritizes_killable_target_over_nearer_healthy_one_on_end_turn },
    { TEST_NAME("game_ai_retreats_when_badly_wounded_on_end_turn"), test_game_ai_retreats_when_badly_wounded_on_end_turn },
    { TEST_NAME("game_ai_retreat_prefers_farther_tile_and_skips_unwalkable_neighbor_on_end_turn"), test_game_ai_retreat_prefers_farther_tile_and_skips_unwalkable_neighbor_on_end_turn },
    { TEST_NAME("game_ai_retreats_vertically_away_from_target_on_end_turn"), test_game_ai_retreats_vertically_away_from_target_on_end_turn },
    { TEST_NAME("game_ai_retreat_noops_when_fully_boxed_in_on_end_turn"), test_game_ai_retreat_noops_when_fully_boxed_in_on_end_turn },
    { TEST_NAME("game_ai_takes_lethal_shot_instead_of_retreating_on_end_turn"), test_game_ai_takes_lethal_shot_instead_of_retreating_on_end_turn },
    { TEST_NAME("game_ai_multi_action_turn_after_moving_into_range_with_insufficient_ap_on_end_turn"), test_game_ai_multi_action_turn_after_moving_into_range_with_insufficient_ap_on_end_turn },
    { TEST_NAME("game_ai_multi_action_turn_runs_full_action_budget_on_end_turn"), test_game_ai_multi_action_turn_runs_full_action_budget_on_end_turn },
};

const uint32_t g_game_ai_tests_count = sizeof(g_game_ai_tests) / sizeof(g_game_ai_tests[0]);
