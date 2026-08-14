#include "test_game_combat.h"
#include "lib/assert.h"
#include "game/entity.h"
#include "game/skill.h"
#include "game/grid.h"
#include "game/turn.h"
#include "test_game_helpers.h"

PRIVATE void test_game_attack_kills_defender_clamps_hp_and_frees_tile_for_movement(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 5);
    entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 3, 2, 3);
    entity_t* e2 = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 3}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *e_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e->skills = (slice_skill_t){ .begin = e_skills_begin, .end = skills.end };
    skill_t *e2_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e2->skills = (slice_skill_t){ .begin = e2_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, e);
    turn_order_add(allocator, &order, e2);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);
    test_click_tile(&game, allocator, e->position);

    // Attack damage (5) exceeds e's hp (3): hp clamps to 0, not negative.
    assert_test(e->hp == 0);
    assert_test(!e->alive);
    assert_test(p->ap == 1);
    // e2 is still alive, so the enemy team isn't wiped out yet.
    assert_test(game.game_over == GAME_OVER_NONE);

    // e's corpse no longer occupies its tile: pressing it now falls through
    // to a move, same as any other empty tile.
    test_click_tile(&game, allocator, (position_t){1, 0});

    entity_t *entity = p;
    assert_test(entity->position.x == 1 && entity->position.y == 0);
    assert_test(entity->mp == 4);

    game_deinit(allocator, game);
}

PRIVATE void test_game_entity_pressed_diagonal_and_far_enemy_attack_noop(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 6, 6);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* diagonal = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 1}, 10, 2, 3);
    entity_t* far = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){5, 5}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *diagonal_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    diagonal->skills = (slice_skill_t){ .begin = diagonal_skills_begin, .end = skills.end };
    skill_t *far_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    far->skills = (slice_skill_t){ .begin = far_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, diagonal);
    turn_order_add(allocator, &order, far);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    assert_test(game.mode == GAME_MODE_MOVEMENT && turn_active_entity(game.turn) == p);

    test_click_attack_toggle(&game, allocator);
    assert_test(game.mode == GAME_MODE_ATTACK);

    // Diagonal doesn't count as adjacent: pressing it is a no-op.
    test_click_tile(&game, allocator, diagonal->position);
    assert_test(p->ap == 2);
    assert_test(diagonal->hp == 10);

    // Neither does simply being far away.
    test_click_tile(&game, allocator, far->position);
    assert_test(p->ap == 2);
    assert_test(far->hp == 10);

    game_deinit(allocator, game);
}

PRIVATE void test_game_turn_order_compacts_when_non_active_entity_dies_during_attack(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* b = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 3}, 10, 2, 3);
    entity_t* c = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 3, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *b_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    b->skills = (slice_skill_t){ .begin = b_skills_begin, .end = skills.end };
    skill_t *c_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    c->skills = (slice_skill_t){ .begin = c_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, b);
    turn_order_add(allocator, &order, c);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);
    test_click_tile(&game, allocator, c->position);

    assert_test(!c->alive);
    assert_test(SLICE_TYPESIZE(game.turn.order) == 2);
    assert_test(SLICE_AT(game.turn.order, 0) == p);
    assert_test(SLICE_AT(game.turn.order, 1) == b);
    assert_test(turn_active_entity(game.turn) == p);

    game_deinit(allocator, game);
}

PRIVATE void test_game_entity_pressed_adjacent_enemy_attacks_then_noops_when_ap_zero(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 1, 3);
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

    test_click_tile(&game, allocator, p->position);
    assert_test(game.mode == GAME_MODE_MOVEMENT && turn_active_entity(game.turn) == p);

    test_click_attack_toggle(&game, allocator);
    test_click_tile(&game, allocator, e->position);
    assert_test(p->ap == 0);
    assert_test(e->hp == 5);
    assert_test(e->alive);
    // A successful attack closes attack mode automatically.
    assert_test(game.mode == GAME_MODE_MOVEMENT);

    test_click_attack_toggle(&game, allocator);
    test_click_tile(&game, allocator, e->position);
    assert_test(p->ap == 0);
    assert_test(e->hp == 5);

    game_deinit(allocator, game);
}

PRIVATE void test_game_ranged_attack_hits_at_max_range_without_moving(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 5, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 1, 3);
    entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 0}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_RANGED);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *e_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e->skills = (slice_skill_t){ .begin = e_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, e);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    assert_test(game.mode == GAME_MODE_MOVEMENT && turn_active_entity(game.turn) == p);

    test_click_attack_toggle(&game, allocator);

    // e sits exactly 3 tiles away (SKILL_RANGED.range): reachable without moving.
    test_click_tile(&game, allocator, e->position);

    assert_test(e->hp == 7);
    assert_test(p->ap == 0);
    assert_test(p->position.x == 0 && p->position.y == 0);

    game_deinit(allocator, game);
}

PRIVATE void test_game_ranged_attack_noop_beyond_range(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 5, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 1, 3);
    entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){4, 0}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_RANGED);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *e_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e->skills = (slice_skill_t){ .begin = e_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, e);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);

    // e sits 4 tiles away, one beyond SKILL_RANGED.range (3): out of reach.
    test_click_tile(&game, allocator, e->position);

    assert_test(e->hp == 10);
    assert_test(p->ap == 1);

    game_deinit(allocator, game);
}

// Ticket 003: attack-range BFS now treats entities on the opposing team of
// the attacker as passable, so an enemy standing on the only straight-line
// path no longer blocks a shot at another enemy behind it -- this is the
// bug feature 1 fixes (previously this exact scenario asserted the attack
// was blocked; it's now expected to land). Allies of the attacker are a
// separate scenario below (test_game_ranged_attack_still_blocked_by_ally_in_path)
// and remain blocking, unchanged.
PRIVATE void test_game_ranged_attack_reaches_through_enemy_in_path(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 1, 3);
    entity_t* blocker = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){2, 0}, 10, 1, 3);
    entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 0}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_RANGED);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *blocker_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    blocker->skills = (slice_skill_t){ .begin = blocker_skills_begin, .end = skills.end };
    skill_t *e_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e->skills = (slice_skill_t){ .begin = e_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, blocker);
    turn_order_add(allocator, &order, e);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);

    // e is within range (3); blocker (an enemy of p, same as e) occupies the
    // only tile on the straight-line path (height-1 grid, no way around) but
    // is now passable for range purposes: the attack on e lands.
    assert_test(test_tile_list_contains(game.render.attack_range_tiles, e->position));
    test_click_tile(&game, allocator, e->position);

    assert_test(e->hp == 10 - SKILL_RANGED.damage);
    assert_test(p->ap == 0);
    // blocker itself wasn't the target: untouched.
    assert_test(blocker->hp == 10);

    game_deinit(allocator, game);
}

// Allies still block occupancy in the attack-range BFS -- only the
// attacker's opposing team is made passable (PLAN.md Q2: scoped to what the
// feature actually asked for, not "ignore everyone").
PRIVATE void test_game_ranged_attack_still_blocked_by_ally_in_path(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 1, 3);
    entity_t* ally = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){2, 0}, 10, 1, 3);
    entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 0}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_RANGED);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *ally_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    ally->skills = (slice_skill_t){ .begin = ally_skills_begin, .end = skills.end };
    skill_t *e_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e->skills = (slice_skill_t){ .begin = e_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, ally);
    turn_order_add(allocator, &order, e);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);

    // e is within range (3), but ally occupies the only tile on the
    // straight-line path: still unreachable, since allies aren't passable.
    assert_test(!test_tile_list_contains(game.render.attack_range_tiles, e->position));
    test_click_tile(&game, allocator, e->position);

    assert_test(e->hp == 10);
    assert_test(p->ap == 1);
    assert_test(ally->hp == 10);

    game_deinit(allocator, game);
}

// attack_range_tiles itself (not just a specific attack outcome) now
// includes both the occupied enemy's own tile AND the tile beyond it, once
// that tile is within range through the now-passable enemy -- the exact
// "cells under/behind an enemy stay reachable-for-targeting" behavior
// feature 1 asks for.
PRIVATE void test_game_attack_range_tiles_include_occupied_and_beyond(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 5, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 1, 3);
    entity_t* enemy_at_range_2 = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){2, 0}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_RANGED);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *enemy_at_range_2_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    enemy_at_range_2->skills = (slice_skill_t){ .begin = enemy_at_range_2_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, enemy_at_range_2);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);

    // enemy_at_range_2's own tile (distance 2, within SKILL_RANGED.range=3):
    // previously never appeared in attack_range_tiles at all (occupied tiles
    // were skipped before getting a BFS distance).
    assert_test(test_tile_list_contains(game.render.attack_range_tiles, enemy_at_range_2->position));
    // (3, 0), distance 3 through the now-passable enemy: previously
    // unreachable since the enemy blocked the only path past it.
    assert_test(test_tile_list_contains(game.render.attack_range_tiles, (position_t){3, 0}));
    // (4, 0), distance 4, beyond SKILL_RANGED.range=3: still out of range,
    // the fix doesn't extend range, only what occupancy blocks within it.
    assert_test(!test_tile_list_contains(game.render.attack_range_tiles, (position_t){4, 0}));

    game_deinit(allocator, game);
}

PRIVATE void test_game_attack_toggle_after_move_selection_does_not_overflow_scratch(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    // Centered with room to spare in every direction, and mp == SKILL_RANGED.range (3),
    // so both diamonds are the same, uncropped, maximum size.
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){8, 5}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_list_add(allocator, &skills, SKILL_RANGED);
    p->skills = skills;

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    // Selecting first populates render.reachable_tiles (mp=3, a ~24-tile
    // diamond) in game->scratch.
    test_click_tile(&game, allocator, p->position);
    assert_test(SLICE_TYPESIZE(game.render.reachable_tiles) > 0);
    assert_test(SLICE_TYPESIZE(game.render.attack_range_tiles) == 0);

    // Toggling attack mode next computes a same-sized attack_range_tiles
    // diamond (skill.range=3). reachable_tiles and attack_range_tiles are
    // mutually exclusive, so reachable_tiles must be nullified first --
    // scratch never has to fit both diamonds at once.
    expect_panic_begin();
    test_click_attack_toggle(&game, allocator);
    assert_test(!expect_panic_end());

    assert_test(game.mode == GAME_MODE_ATTACK);
    assert_test(SLICE_TYPESIZE(game.render.attack_range_tiles) > 0);
    assert_test(SLICE_TYPESIZE(game.render.reachable_tiles) == 0);

    // Toggling back off restores reachable_tiles and nullifies attack_range_tiles.
    test_click_attack_toggle(&game, allocator);
    assert_test(game.mode == GAME_MODE_MOVEMENT);
    assert_test(SLICE_TYPESIZE(game.render.reachable_tiles) > 0);
    assert_test(SLICE_TYPESIZE(game.render.attack_range_tiles) == 0);

    game_deinit(allocator, game);
}

PRIVATE void test_game_selecting_shows_reachable_tiles_and_toggle_shows_attack_range_tiles(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    // mp (1) is well short of SKILL_RANGED.range (3), so a tile 3 steps away
    // is within skill range but unambiguously out of move range.
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){5, 5}, 10, 2, 1);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_list_add(allocator, &skills, SKILL_RANGED);
    p->skills = skills;

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    position_t adjacent_tile = { 6, 5 };  // 1 tile away: within mp, within skill range
    position_t far_tile = { 8, 5 };       // 3 tiles away: within skill range, beyond mp

    // Pressing the entity selects it: reachable tiles (move range) become
    // visible, attack range tiles stay hidden.
    test_click_tile(&game, allocator, p->position);
    assert_test(test_tile_list_contains(game.render.reachable_tiles, adjacent_tile));
    assert_test(!test_tile_list_contains(game.render.reachable_tiles, far_tile));
    assert_test(SLICE_TYPESIZE(game.render.attack_range_tiles) == 0);

    // Pressing the attack toggle flips visibility: attack range tiles (skill
    // range) become visible, reachable tiles are hidden.
    test_click_attack_toggle(&game, allocator);
    assert_test(test_tile_list_contains(game.render.attack_range_tiles, adjacent_tile));
    assert_test(test_tile_list_contains(game.render.attack_range_tiles, far_tile));
    assert_test(SLICE_TYPESIZE(game.render.reachable_tiles) == 0);

    game_deinit(allocator, game);
}

// Ticket 006: clicking a skill button switches the active entity's selected
// skill and recomputes attack_range_tiles for the new skill's range.
PRIVATE void test_game_skill_button_switches_attack_range_to_selected_skill(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 5, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_list_add(allocator, &skills, SKILL_MELEE);
    skill_list_add(allocator, &skills, SKILL_RANGED);
    p->skills = skills;

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);

    // skills[0] (SKILL_MELEE, range 1) is selected by default: only the
    // adjacent tile is in range, not the far one.
    assert_test(game.selected_skill == 0);
    assert_test(test_tile_list_contains(game.render.attack_range_tiles, (position_t){1, 0}));
    assert_test(!test_tile_list_contains(game.render.attack_range_tiles, (position_t){3, 0}));

    test_click_skill_button(&game, allocator, 1);

    // skills[1] (SKILL_RANGED, range 3) now selected: range extends.
    assert_test(game.selected_skill == 1);
    assert_test(test_tile_list_contains(game.render.attack_range_tiles, (position_t){1, 0}));
    assert_test(test_tile_list_contains(game.render.attack_range_tiles, (position_t){3, 0}));

    game_deinit(allocator, game);
}

// Skill-button clicks no-op outside their valid conditions: mode NONE (no
// selection made yet), and an index the entity doesn't have that many
// skills for (single-skill entity, button index 1).
// entity_spawn requires all spawns to happen contiguously right after
// entity_list_init (assert_debug(allocator->cursor == entities->end) in
// entity.c), so all three entities used here -- two multi-skill, one
// single-skill -- must be spawned upfront, before skill_list_init/turn_order_init.
PRIVATE void test_game_skill_button_pressed_noop_outside_valid_conditions(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 5, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 1, 3);
    // Single-skill entity, to test the out-of-range button index (1) noops
    // for an entity that doesn't have that many skills.
    entity_t* p2 = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){4, 0}, 10, 1, 3);
    // Multi-skill enemy, to test that a non-player active entity ignores
    // skill-button clicks entirely (both game_on_skill_button_pressed's own
    // team check, and the input-dispatch hit-test gating in front of it).
    entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){2, 0}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    skill_list_add(allocator, &skills, SKILL_RANGED);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *p2_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p2->skills = (slice_skill_t){ .begin = p2_skills_begin, .end = skills.end };
    skill_t *e_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    skill_list_add(allocator, &skills, SKILL_RANGED);
    e->skills = (slice_skill_t){ .begin = e_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, e);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, p2);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    // e is active first (turn order: e, p, p2) and isn't player-controlled:
    // click is a no-op regardless of e having multiple skills.
    assert_test(turn_active_entity(game.turn) == e);
    test_click_skill_button(&game, allocator, 1);
    assert_test(game.selected_skill == 0);
    assert_test(turn_active_entity(game.turn) == e);

    game_deinit(allocator, game);
}

PRIVATE void test_game_skill_button_pressed_noop_when_mode_none_or_index_out_of_range(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 5, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 1, 3);
    // Single-skill entity, to test the out-of-range button index (1) noops
    // for an entity that doesn't have that many skills.
    entity_t* p2 = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){4, 0}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    skill_list_add(allocator, &skills, SKILL_RANGED);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *p2_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p2->skills = (slice_skill_t){ .begin = p2_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, p2);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    // GAME_MODE_NONE: nothing selected yet, click is a no-op.
    test_click_skill_button(&game, allocator, 1);
    assert_test(game.selected_skill == 0);

    test_click_end_turn(&game, allocator); // p -> p2, no attack toggle pressed
    assert_test(turn_active_entity(game.turn) == p2);
    test_click_tile(&game, allocator, p2->position);
    assert_test(game.mode == GAME_MODE_MOVEMENT);

    test_click_skill_button(&game, allocator, 1); // entity_skill_count(p2) == 1: index 1 is out of range
    assert_test(game.selected_skill == 0);

    game_deinit(allocator, game);
}

// Attacking after switching skills uses the newly selected skill's
// damage/ap_cost, not the default one.
PRIVATE void test_game_attack_after_skill_switch_uses_new_skill_damage_and_ap_cost(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 5, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 1, 3);
    // Distance 3: out of SKILL_MELEE.range (1), within SKILL_RANGED.range (3).
    entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 0}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    skill_list_add(allocator, &skills, SKILL_RANGED);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *e_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e->skills = (slice_skill_t){ .begin = e_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, e);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);

    // Still on melee (default): e is out of range, clicking it is a no-op.
    test_click_tile(&game, allocator, e->position);
    assert_test(e->hp == 10);
    assert_test(p->ap == 1);

    test_click_skill_button(&game, allocator, 1); // switch to SKILL_RANGED
    test_click_tile(&game, allocator, e->position);

    assert_test(e->hp == 10 - SKILL_RANGED.damage);
    assert_test(p->ap == 1 - SKILL_RANGED.ap_cost);

    game_deinit(allocator, game);
}

const test_case_t g_game_combat_tests[] = {
    { TEST_NAME("game_attack_kills_defender_clamps_hp_and_frees_tile_for_movement"), test_game_attack_kills_defender_clamps_hp_and_frees_tile_for_movement },
    { TEST_NAME("game_entity_pressed_diagonal_and_far_enemy_attack_noop"), test_game_entity_pressed_diagonal_and_far_enemy_attack_noop },
    { TEST_NAME("game_turn_order_compacts_when_non_active_entity_dies_during_attack"), test_game_turn_order_compacts_when_non_active_entity_dies_during_attack },
    { TEST_NAME("game_entity_pressed_adjacent_enemy_attacks_then_noops_when_ap_zero"), test_game_entity_pressed_adjacent_enemy_attacks_then_noops_when_ap_zero },
    { TEST_NAME("game_ranged_attack_hits_at_max_range_without_moving"), test_game_ranged_attack_hits_at_max_range_without_moving },
    { TEST_NAME("game_ranged_attack_noop_beyond_range"), test_game_ranged_attack_noop_beyond_range },
    { TEST_NAME("game_ranged_attack_reaches_through_enemy_in_path"), test_game_ranged_attack_reaches_through_enemy_in_path },
    { TEST_NAME("game_ranged_attack_still_blocked_by_ally_in_path"), test_game_ranged_attack_still_blocked_by_ally_in_path },
    { TEST_NAME("game_attack_range_tiles_include_occupied_and_beyond"), test_game_attack_range_tiles_include_occupied_and_beyond },
    { TEST_NAME("game_attack_toggle_after_move_selection_does_not_overflow_scratch"), test_game_attack_toggle_after_move_selection_does_not_overflow_scratch },
    { TEST_NAME("game_selecting_shows_reachable_tiles_and_toggle_shows_attack_range_tiles"), test_game_selecting_shows_reachable_tiles_and_toggle_shows_attack_range_tiles },
    { TEST_NAME("game_skill_button_switches_attack_range_to_selected_skill"), test_game_skill_button_switches_attack_range_to_selected_skill },
    { TEST_NAME("game_skill_button_pressed_noop_outside_valid_conditions"), test_game_skill_button_pressed_noop_outside_valid_conditions },
    { TEST_NAME("game_skill_button_pressed_noop_when_mode_none_or_index_out_of_range"), test_game_skill_button_pressed_noop_when_mode_none_or_index_out_of_range },
    { TEST_NAME("game_attack_after_skill_switch_uses_new_skill_damage_and_ap_cost"), test_game_attack_after_skill_switch_uses_new_skill_damage_and_ap_cost },
};

const uint32_t g_game_combat_tests_count = sizeof(g_game_combat_tests) / sizeof(g_game_combat_tests[0]);
