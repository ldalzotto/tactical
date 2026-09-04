#include "test_game_combat.h"
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

    // Damage (5) exceeds e's hp (3): hp clamps to 0, not negative.
    assert_test(e->hp == 0);
    assert_test(!e->alive);
    assert_test(p->ap == 1);
    // e2 still alive: enemy team not wiped out yet.
    assert_test(game.game_over == GAME_OVER_NONE);

    // e's corpse frees its tile: pressing it now falls through to a move.
    test_click_tile(&game, allocator, (position_t){1, 0});

    entity_t *entity = p;
    assert_test(entity->position.x == 1);
    assert_test(entity->position.y == 0);
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
    assert_test(game.mode == GAME_MODE_MOVEMENT);
    assert_test(turn_active_entity(game.turn) == p);

    test_click_attack_toggle(&game, allocator);
    assert_test(game.mode == GAME_MODE_ATTACK);

    // Diagonal isn't adjacent: press is a no-op.
    test_click_tile(&game, allocator, diagonal->position);
    assert_test(p->ap == 2);
    assert_test(diagonal->hp == 10);

    // Nor does being far away.
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
    assert_test(game.mode == GAME_MODE_MOVEMENT);
    assert_test(turn_active_entity(game.turn) == p);

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
    assert_test(game.mode == GAME_MODE_MOVEMENT);
    assert_test(turn_active_entity(game.turn) == p);

    test_click_attack_toggle(&game, allocator);

    // e is exactly SKILL_RANGED.range away: reachable without moving.
    test_click_tile(&game, allocator, e->position);

    assert_test(e->hp == 7);
    assert_test(p->ap == 0);
    assert_test(p->position.x == 0);
    assert_test(p->position.y == 0);

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

    // e is one tile beyond SKILL_RANGED.range: out of reach.
    test_click_tile(&game, allocator, e->position);

    assert_test(e->hp == 10);
    assert_test(p->ap == 1);

    game_deinit(allocator, game);
}

// Any entity -- ally or enemy -- occludes the attack-range BFS: a target
// is an obstacle, not a window, so an enemy on the only straight-line
// path still blocks a shot at one behind it (same rule as the ally case in
// test_game_ranged_attack_still_blocked_by_ally_in_path below).
PRIVATE void test_game_ranged_attack_blocked_by_enemy_in_path(linear_allocator_t *allocator) {
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

    // blocker occupies the only path tile (height-1 grid, no way around):
    // its own tile is reachable-for-targeting, but it occludes everything
    // behind it, so e never enters attack_range_tiles and attacking e
    // is illegal.
    assert_test(test_tile_list_contains(game.pathing.attack_range_tiles, blocker->position));
    assert_test(!test_tile_list_contains(game.pathing.attack_range_tiles, e->position));
    test_click_tile(&game, allocator, e->position);

    assert_test(e->hp == 10);
    assert_test(p->ap == 1);
    assert_test(blocker->hp == 10);

    game_deinit(allocator, game);
}

// Allies still block occupancy in the attack-range BFS -- only the
// opposing team is made passable.
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

    // e is in range, but ally occupies the only path tile: unreachable,
    // since allies aren't passable.
    assert_test(!test_tile_list_contains(game.pathing.attack_range_tiles, e->position));
    test_click_tile(&game, allocator, e->position);

    assert_test(e->hp == 10);
    assert_test(p->ap == 1);
    assert_test(ally->hp == 10);

    game_deinit(allocator, game);
}

// attack_range_tiles includes an occupied tile itself (targetable), but
// the entity still occludes the BFS: nothing behind it enters
// attack_range_tiles even if otherwise in range.
PRIVATE void test_game_attack_range_tiles_include_occupied_but_not_beyond(linear_allocator_t *allocator) {
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

    // enemy_at_range_2's tile: reachable-for-targeting despite being occupied.
    assert_test(test_tile_list_contains(game.pathing.attack_range_tiles, enemy_at_range_2->position));
    // (3,0): occluded by enemy_at_range_2, the only path tile -- never
    // enters attack_range_tiles regardless of range.
    assert_test(!test_tile_list_contains(game.pathing.attack_range_tiles, (position_t){3, 0}));
    // (4,0): also occluded, and out of range regardless.
    assert_test(!test_tile_list_contains(game.pathing.attack_range_tiles, (position_t){4, 0}));

    game_deinit(allocator, game);
}

// A wall doesn't block walking here (a same-length detour exists), but
// must still block LOS: the straight ray crosses the wall even though a
// walking path of equal length goes around it.
PRIVATE void test_game_ranged_attack_blocked_by_wall_on_diagonal_line(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 3, 3);
    grid_set_walkable(grid, (position_t){1, 1}, false);
    grid_set_blocks_sight(grid, (position_t){1, 1}, true);

    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){2, 2}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    // range=4: e at Manhattan distance 4; the diagonal ray crosses the
    // wall at (1,1), but a walking detour is also exactly 4 steps -- stays
    // out of range only if LOS (not walking distance) gates it.
    skill_list_add(allocator, &skills, (skill_t){ .range = 4, .damage = 3, .ap_cost = 1 });
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

    assert_test(!test_tile_list_contains(game.pathing.attack_range_tiles, e->position));
    test_click_tile(&game, allocator, e->position);

    assert_test(e->hp == 10);
    assert_test(p->ap == 2);

    game_deinit(allocator, game);
}

// The line from (0,0) to (2,1) hits a genuine Bresenham diagonal tie at
// x=1: the standard path steps through (1,0), but the other tile the
// continuous ray grazes at that tie is (1,1) -- geometry_line_iter_start's
// prefer_y_step path. A wall at (1,1) alone doesn't sit on the standard
// path, so checking only that path (the old behavior) would call this
// shot clear; requiring both tie-break paths to be clear catches it.
PRIVATE void test_game_ranged_attack_blocked_by_wall_on_tie_break_path_only(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 3, 2);
    grid_set_walkable(grid, (position_t){1, 1}, false);
    grid_set_blocks_sight(grid, (position_t){1, 1}, true);

    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){2, 1}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, (skill_t){ .range = 3, .damage = 3, .ap_cost = 1 });
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

    assert_test(!test_tile_list_contains(game.pathing.attack_range_tiles, e->position));
    test_click_tile(&game, allocator, e->position);

    assert_test(e->hp == 10);
    assert_test(p->ap == 2);

    game_deinit(allocator, game);
}

// Mirror of the above along the same physical line, attacking the other
// way: (2,1) -> (0,0) swaps which tile is on the standard path vs. the
// tie-break alternate (standard now hits (1,1), alt hits (1,0)), so a
// wall at (1,0) alone is only caught via the tie-break path here too.
// Confirms the fix isn't direction-dependent -- before it, LOS could be
// clear from A to B but blocked from B to A along the identical line.
PRIVATE void test_game_ranged_attack_blocked_by_wall_on_tie_break_path_only_reverse_direction(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 3, 2);
    grid_set_walkable(grid, (position_t){1, 0}, false);
    grid_set_blocks_sight(grid, (position_t){1, 0}, true);

    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){2, 1}, 10, 2, 3);
    entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){0, 0}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, (skill_t){ .range = 3, .damage = 3, .ap_cost = 1 });
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

    assert_test(!test_tile_list_contains(game.pathing.attack_range_tiles, e->position));
    test_click_tile(&game, allocator, e->position);

    assert_test(e->hp == 10);
    assert_test(p->ap == 2);

    game_deinit(allocator, game);
}

// Excluded from attack_range_tiles but still shown (dimmed) via
// los_blocked_tiles, unlike a hidden ally-occupied tile.
PRIVATE void test_game_los_blocked_tile_visible_but_not_selectable(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 3, 3);
    grid_set_walkable(grid, (position_t){1, 1}, false);
    grid_set_blocks_sight(grid, (position_t){1, 1}, true);

    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){2, 2}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, (skill_t){ .range = 4, .damage = 3, .ap_cost = 1 });
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

    assert_test(!test_tile_list_contains(game.pathing.attack_range_tiles, e->position));
    assert_test(test_tile_list_contains(game.pathing.los_blocked_tiles, e->position));

    game_deinit(allocator, game);
}

// Ally-occupied tiles are hidden entirely: excluded from both
// attack_range_tiles and los_blocked_tiles.
PRIVATE void test_game_ally_occupied_tile_excluded_from_both_lists(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 3, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* ally = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){1, 0}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_RANGED);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *ally_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    ally->skills = (slice_skill_t){ .begin = ally_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, ally);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);

    assert_test(!test_tile_list_contains(game.pathing.attack_range_tiles, ally->position));
    assert_test(!test_tile_list_contains(game.pathing.los_blocked_tiles, ally->position));

    game_deinit(allocator, game);
}

// A non-walkable tile that doesn't block sight (chasm, low wall) must not
// shrink attack range: range is Manhattan distance + LOS, not walking
// distance, so a target in plain sight across it is still legal.
PRIVATE void test_game_ranged_attack_not_blocked_by_non_walkable_sight_clear_tile(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 5, 3);
    grid_set_walkable(grid, (position_t){1, 1}, false);

    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 1}, 10, 3, 2);
    entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){2, 1}, 10, 2, 3);

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

    // e is within range; the ray crosses only (1,1), non-walkable but not
    // sight-blocking -- e stays a legal target.
    assert_test(test_tile_list_contains(game.pathing.attack_range_tiles, e->position));
    test_click_tile(&game, allocator, e->position);

    assert_test(e->hp == 10 - SKILL_RANGED.damage);
    assert_test(p->ap == 3 - SKILL_RANGED.ap_cost);

    game_deinit(allocator, game);
}

// Empty grass (walkable but sight-blocking) must not enter
// attack_range_tiles even in range -- nothing there to target.
PRIVATE void test_game_attack_range_excludes_empty_sight_blocking_tile(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 3, 1);
    grid_set_tile(grid, (position_t){1, 0}, TILE_GRASS);

    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_RANGED);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);

    assert_test(!test_tile_list_contains(game.pathing.attack_range_tiles, (position_t){1, 0}));

    game_deinit(allocator, game);
}

// An entity on grass doesn't occlude itself -- unlike empty grass, it
// stays targetable.
PRIVATE void test_game_attack_range_includes_entity_on_sight_blocking_tile(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 3, 1);
    grid_set_tile(grid, (position_t){1, 0}, TILE_GRASS);

    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 1, 3);
    entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 10, 1, 3);

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

    assert_test(test_tile_list_contains(game.pathing.attack_range_tiles, e->position));
    test_click_tile(&game, allocator, e->position);

    assert_test(e->hp == 10 - SKILL_RANGED.damage);
    assert_test(p->ap == 1 - SKILL_RANGED.ap_cost);

    game_deinit(allocator, game);
}

// Range is a Manhattan-distance boundary: exactly max_range is in, one
// step further is out, open ground, no occlusion.
PRIVATE void test_game_attack_range_manhattan_boundary(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 5, 4);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 3, 1);
    // South, at exactly SKILL_RANGED.range.
    entity_t* in_range = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){0, 3}, 10, 1, 3);
    // East, at range + 1 -- different axis from in_range so rays don't cross.
    entity_t* out_of_range = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){4, 0}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_RANGED);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *in_range_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    in_range->skills = (slice_skill_t){ .begin = in_range_skills_begin, .end = skills.end };
    skill_t *out_of_range_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    out_of_range->skills = (slice_skill_t){ .begin = out_of_range_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, in_range);
    turn_order_add(allocator, &order, out_of_range);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);

    assert_test(test_tile_list_contains(game.pathing.attack_range_tiles, in_range->position));
    assert_test(!test_tile_list_contains(game.pathing.attack_range_tiles, out_of_range->position));

    test_click_tile(&game, allocator, out_of_range->position);
    assert_test(out_of_range->hp == 10);
    assert_test(p->ap == 3);

    test_click_tile(&game, allocator, in_range->position);
    assert_test(in_range->hp == 10 - SKILL_RANGED.damage);
    assert_test(p->ap == 3 - SKILL_RANGED.ap_cost);

    game_deinit(allocator, game);
}

PRIVATE void test_game_attack_toggle_after_move_selection_does_not_overflow_scratch(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    // Centered with room to spare, mp == SKILL_RANGED.range, so both
    // diamonds are the same, uncropped, maximum size.
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){8, 5}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_list_add(allocator, &skills, SKILL_RANGED);
    p->skills = skills;

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    // Selecting first populates the walking_distances overlay in
    // game->scratch.
    test_click_tile(&game, allocator, p->position);
    assert_test(test_reachable_tile_count(&game) > 0);
    assert_test(SLICE_TYPESIZE(game.pathing.attack_range_tiles) == 0);

    // Toggling attack mode computes a same-sized attack_range_tiles
    // diamond. walking_distances/attack_range_tiles are mutually
    // exclusive, so walking_distances is nullified first -- scratch never
    // fits both diamonds at once.
    expect_panic_begin();
    test_click_attack_toggle(&game, allocator);
    assert_test(!expect_panic_end());

    assert_test(game.mode == GAME_MODE_ATTACK);
    assert_test(SLICE_TYPESIZE(game.pathing.attack_range_tiles) > 0);
    assert_test(SLICE_TYPESIZE(game.pathing.walking_distances.dist) == 0);

    // Toggling off restores walking_distances and nullifies attack_range_tiles.
    test_click_attack_toggle(&game, allocator);
    assert_test(game.mode == GAME_MODE_MOVEMENT);
    assert_test(test_reachable_tile_count(&game) > 0);
    assert_test(SLICE_TYPESIZE(game.pathing.attack_range_tiles) == 0);

    game_deinit(allocator, game);
}

// Regression: this skill range used to overflow game->scratch's old fixed
// 256-byte capacity. It now grows on demand (pathing_ranges_push_attack_range
// in pathing_ranges.c), so this must not panic.
PRIVATE void test_game_attack_toggle_with_large_range_skill_grows_scratch(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){8, 5}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_list_add(allocator, &skills, (skill_t){ .range = 20, .ap_cost = 1, .damage = 2 });
    p->skills = skills;

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    test_click_tile(&game, allocator, p->position);

    expect_panic_begin();
    test_click_attack_toggle(&game, allocator);
    assert_test(!expect_panic_end());

    assert_test(game.mode == GAME_MODE_ATTACK);
    // Old capacity was 32 tiles (256 bytes / sizeof(position_t)); a
    // range-20 diamond on a 16x10 grid exceeds that.
    assert_test(SLICE_TYPESIZE(game.pathing.attack_range_tiles) > 32);

    game_deinit(allocator, game);
}

PRIVATE void test_game_selecting_shows_reachable_tiles_and_toggle_shows_attack_range_tiles(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    // mp is well short of SKILL_RANGED.range, so a tile 3 steps away is in
    // skill range but out of move range.
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

    // Selecting the entity shows reachable tiles (move range); attack
    // range stays hidden.
    test_click_tile(&game, allocator, p->position);
    assert_test(test_position_reachable(&game, adjacent_tile));
    assert_test(!test_position_reachable(&game, far_tile));
    assert_test(SLICE_TYPESIZE(game.pathing.attack_range_tiles) == 0);

    // Attack toggle flips visibility: attack range shows, reachable tiles hide.
    test_click_attack_toggle(&game, allocator);
    assert_test(test_tile_list_contains(game.pathing.attack_range_tiles, adjacent_tile));
    assert_test(test_tile_list_contains(game.pathing.attack_range_tiles, far_tile));
    assert_test(SLICE_TYPESIZE(game.pathing.walking_distances.dist) == 0);

    game_deinit(allocator, game);
}

// Ticket 006: clicking a skill button switches the selected skill and
// recomputes attack_range_tiles for its range.
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

    // skills[0] (SKILL_MELEE, range 1) selected by default: only adjacent
    // is in range.
    assert_test(game.selected_skill == 0);
    assert_test(test_tile_list_contains(game.pathing.attack_range_tiles, (position_t){1, 0}));
    assert_test(!test_tile_list_contains(game.pathing.attack_range_tiles, (position_t){3, 0}));

    test_click_skill_button(&game, allocator, 1);

    // skills[1] (SKILL_RANGED) now selected: range extends.
    assert_test(game.selected_skill == 1);
    assert_test(test_tile_list_contains(game.pathing.attack_range_tiles, (position_t){1, 0}));
    assert_test(test_tile_list_contains(game.pathing.attack_range_tiles, (position_t){3, 0}));

    game_deinit(allocator, game);
}

// Skill-button clicks no-op outside valid conditions: mode NONE, or an
// out-of-range skill index. entity_spawn requires spawns contiguous right
// after entity_list_init (assert_debug in entity.c), so all three entities
// here must be spawned upfront, before skill_list_init/turn_order_init.
PRIVATE void test_game_skill_button_pressed_noop_outside_valid_conditions(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 5, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 1, 3);
    // Single-skill entity: tests that button index 1 noops when the
    // entity lacks that many skills.
    entity_t* p2 = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){4, 0}, 10, 1, 3);
    // Multi-skill enemy: tests that a non-player active entity ignores
    // skill-button clicks entirely (game_on_skill_button_pressed's team
    // check, plus the hit-test gate in front of it).
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

    // e is active first and isn't player-controlled: click is a no-op
    // regardless of multiple skills.
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
    // Single-skill entity: tests that button index 1 noops when the
    // entity lacks that many skills.
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

    // GAME_MODE_NONE: nothing selected, click is a no-op.
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
// damage/ap_cost, not the default.
PRIVATE void test_game_attack_after_skill_switch_uses_new_skill_damage_and_ap_cost(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 5, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 1, 3);
    // Distance 3: out of SKILL_MELEE.range, within SKILL_RANGED.range.
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

    // Still on melee (default): e is out of range, click is a no-op.
    test_click_tile(&game, allocator, e->position);
    assert_test(e->hp == 10);
    assert_test(p->ap == 1);

    test_click_skill_button(&game, allocator, 1); // switch to SKILL_RANGED
    test_click_tile(&game, allocator, e->position);

    assert_test(e->hp == 10 - SKILL_RANGED.damage);
    assert_test(p->ap == 1 - SKILL_RANGED.ap_cost);

    game_deinit(allocator, game);
}

PRIVATE void test_game_attack_toggle_then_ally_click_is_noop(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* ally = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){1, 0}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *ally_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    ally->skills = (slice_skill_t){ .begin = ally_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, ally);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);
    assert_test(game.mode == GAME_MODE_ATTACK);

    // Ally's tile is same-team, so pathing_compute_attack_range excludes
    // it from attack_range_tiles -- never a highlighted target.
    assert_test(!test_tile_list_contains(game.pathing.attack_range_tiles, ally->position));

    // In attack mode, clicking an ally routes through action_try_attack,
    // which rejects same-team targets.
    test_click_tile(&game, allocator, ally->position);

    assert_test(p->ap == 2);
    assert_test(ally->hp == 10);
    assert_test(game.mode == GAME_MODE_ATTACK);

    game_deinit(allocator, game);
}

// D1 regression: clicking an empty tile in GAME_MODE_ATTACK must never
// fall through to action_try_move, whether inside the attack overlay or
// only the (hidden) movement range.
PRIVATE void test_game_attack_mode_tile_click_does_not_move(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 6, 1);
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

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);
    assert_test(game.mode == GAME_MODE_ATTACK);

    // (2,0): within move range but outside the melee attack overlay --
    // the tile that used to silently move the entity and drop attack mode.
    assert_test(!test_tile_list_contains(game.pathing.attack_range_tiles, (position_t){2, 0}));
    test_click_tile(&game, allocator, (position_t){2, 0});
    assert_test(p->position.x == 0);
    assert_test(p->position.y == 0);
    assert_test(p->mp == 3);
    assert_test(game.mode == GAME_MODE_ATTACK);

    // (1,0): inside the attack overlay but empty. Still not a valid
    // target, must no-op the same way.
    assert_test(test_tile_list_contains(game.pathing.attack_range_tiles, (position_t){1, 0}));
    test_click_tile(&game, allocator, (position_t){1, 0});
    assert_test(p->position.x == 0);
    assert_test(p->position.y == 0);
    assert_test(p->mp == 3);
    assert_test(game.mode == GAME_MODE_ATTACK);

    game_deinit(allocator, game);
}

// D1 regression guard: toggling attack mode on and off must not disturb
// the legitimate movement-mode tile click path.
PRIVATE void test_game_tile_click_moves_after_toggling_attack_mode_off(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 6, 1);
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

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);
    assert_test(game.mode == GAME_MODE_ATTACK);
    test_click_attack_toggle(&game, allocator);
    assert_test(game.mode == GAME_MODE_MOVEMENT);

    test_click_tile(&game, allocator, (position_t){2, 0});
    assert_test(p->position.x == 2);
    assert_test(p->position.y == 0);
    assert_test(p->mp == 1);
    assert_test(game.mode == GAME_MODE_MOVEMENT);

    game_deinit(allocator, game);
}

const test_case_t g_game_combat_tests[] = {
    { TEST_NAME("game_attack_kills_defender_clamps_hp_and_frees_tile_for_movement"), test_game_attack_kills_defender_clamps_hp_and_frees_tile_for_movement },
    { TEST_NAME("game_entity_pressed_diagonal_and_far_enemy_attack_noop"), test_game_entity_pressed_diagonal_and_far_enemy_attack_noop },
    { TEST_NAME("game_turn_order_compacts_when_non_active_entity_dies_during_attack"), test_game_turn_order_compacts_when_non_active_entity_dies_during_attack },
    { TEST_NAME("game_entity_pressed_adjacent_enemy_attacks_then_noops_when_ap_zero"), test_game_entity_pressed_adjacent_enemy_attacks_then_noops_when_ap_zero },
    { TEST_NAME("game_ranged_attack_hits_at_max_range_without_moving"), test_game_ranged_attack_hits_at_max_range_without_moving },
    { TEST_NAME("game_ranged_attack_noop_beyond_range"), test_game_ranged_attack_noop_beyond_range },
    { TEST_NAME("game_ranged_attack_blocked_by_enemy_in_path"), test_game_ranged_attack_blocked_by_enemy_in_path },
    { TEST_NAME("game_ranged_attack_still_blocked_by_ally_in_path"), test_game_ranged_attack_still_blocked_by_ally_in_path },
    { TEST_NAME("game_attack_range_tiles_include_occupied_but_not_beyond"), test_game_attack_range_tiles_include_occupied_but_not_beyond },
    { TEST_NAME("game_ranged_attack_blocked_by_wall_on_diagonal_line"), test_game_ranged_attack_blocked_by_wall_on_diagonal_line },
    { TEST_NAME("game_ranged_attack_blocked_by_wall_on_tie_break_path_only"), test_game_ranged_attack_blocked_by_wall_on_tie_break_path_only },
    { TEST_NAME("game_ranged_attack_blocked_by_wall_on_tie_break_path_only_reverse_direction"), test_game_ranged_attack_blocked_by_wall_on_tie_break_path_only_reverse_direction },
    { TEST_NAME("game_los_blocked_tile_visible_but_not_selectable"), test_game_los_blocked_tile_visible_but_not_selectable },
    { TEST_NAME("game_ally_occupied_tile_excluded_from_both_lists"), test_game_ally_occupied_tile_excluded_from_both_lists },
    { TEST_NAME("game_ranged_attack_not_blocked_by_non_walkable_sight_clear_tile"), test_game_ranged_attack_not_blocked_by_non_walkable_sight_clear_tile },
    { TEST_NAME("game_attack_range_excludes_empty_sight_blocking_tile"), test_game_attack_range_excludes_empty_sight_blocking_tile },
    { TEST_NAME("game_attack_range_includes_entity_on_sight_blocking_tile"), test_game_attack_range_includes_entity_on_sight_blocking_tile },
    { TEST_NAME("game_attack_range_manhattan_boundary"), test_game_attack_range_manhattan_boundary },
    { TEST_NAME("game_attack_toggle_after_move_selection_does_not_overflow_scratch"), test_game_attack_toggle_after_move_selection_does_not_overflow_scratch },
    { TEST_NAME("game_attack_toggle_with_large_range_skill_grows_scratch"), test_game_attack_toggle_with_large_range_skill_grows_scratch },
    { TEST_NAME("game_selecting_shows_reachable_tiles_and_toggle_shows_attack_range_tiles"), test_game_selecting_shows_reachable_tiles_and_toggle_shows_attack_range_tiles },
    { TEST_NAME("game_skill_button_switches_attack_range_to_selected_skill"), test_game_skill_button_switches_attack_range_to_selected_skill },
    { TEST_NAME("game_skill_button_pressed_noop_outside_valid_conditions"), test_game_skill_button_pressed_noop_outside_valid_conditions },
    { TEST_NAME("game_skill_button_pressed_noop_when_mode_none_or_index_out_of_range"), test_game_skill_button_pressed_noop_when_mode_none_or_index_out_of_range },
    { TEST_NAME("game_attack_after_skill_switch_uses_new_skill_damage_and_ap_cost"), test_game_attack_after_skill_switch_uses_new_skill_damage_and_ap_cost },
    { TEST_NAME("game_attack_toggle_then_ally_click_is_noop"), test_game_attack_toggle_then_ally_click_is_noop },
    { TEST_NAME("game_attack_mode_tile_click_does_not_move"), test_game_attack_mode_tile_click_does_not_move },
    { TEST_NAME("game_tile_click_moves_after_toggling_attack_mode_off"), test_game_tile_click_moves_after_toggling_attack_mode_off },
};

const uint32_t g_game_combat_tests_count = sizeof(g_game_combat_tests) / sizeof(g_game_combat_tests[0]);
