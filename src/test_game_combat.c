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
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 5, SKILL_MELEE);
    entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 3, 2, 3, SKILL_MELEE);
    entity_t* e2 = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 3}, 10, 2, 3, SKILL_MELEE);

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, e);
    turn_order_add(allocator, &order, e2);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, 320, 240, 40);

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
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3, SKILL_MELEE);
    entity_t* diagonal = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 1}, 10, 2, 3, SKILL_MELEE);
    entity_t* far = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){5, 5}, 10, 2, 3, SKILL_MELEE);

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, diagonal);
    turn_order_add(allocator, &order, far);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, 320, 240, 40);

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
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3, SKILL_MELEE);
    entity_t* b = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 3}, 10, 2, 3, SKILL_MELEE);
    entity_t* c = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 3, 2, 3, SKILL_MELEE);

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, b);
    turn_order_add(allocator, &order, c);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, 320, 240, 40);

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
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 1, 3, SKILL_MELEE);
    entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 10, 2, 3, SKILL_MELEE);

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, e);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

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
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 1, 3, SKILL_RANGED);
    entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 0}, 10, 1, 3, SKILL_MELEE);

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, e);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, 320, 240, 40);

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
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 1, 3, SKILL_RANGED);
    entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){4, 0}, 10, 1, 3, SKILL_MELEE);

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, e);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);

    // e sits 4 tiles away, one beyond SKILL_RANGED.range (3): out of reach.
    test_click_tile(&game, allocator, e->position);

    assert_test(e->hp == 10);
    assert_test(p->ap == 1);

    game_deinit(allocator, game);
}

PRIVATE void test_game_ranged_attack_blocked_by_unit_in_path(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 1, 3, SKILL_RANGED);
    entity_t* blocker = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){2, 0}, 10, 1, 3, SKILL_MELEE);
    entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 0}, 10, 1, 3, SKILL_MELEE);

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, blocker);
    turn_order_add(allocator, &order, e);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);

    // e is within range (3), but blocker occupies the only tile on the
    // straight-line path (height-1 grid, no way around): unreachable.
    test_click_tile(&game, allocator, e->position);

    assert_test(e->hp == 10);
    assert_test(p->ap == 1);
    assert_test(blocker->hp == 10);

    game_deinit(allocator, game);
}

PRIVATE void test_game_attack_toggle_after_move_selection_does_not_overflow_scratch(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    // Centered with room to spare in every direction, and mp == SKILL_RANGED.range (3),
    // so both diamonds are the same, uncropped, maximum size.
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){8, 5}, 10, 2, 3, SKILL_RANGED);

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

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
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){5, 5}, 10, 2, 1, SKILL_RANGED);

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

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

const test_case_t g_game_combat_tests[] = {
    { TEST_NAME("game_attack_kills_defender_clamps_hp_and_frees_tile_for_movement"), test_game_attack_kills_defender_clamps_hp_and_frees_tile_for_movement },
    { TEST_NAME("game_entity_pressed_diagonal_and_far_enemy_attack_noop"), test_game_entity_pressed_diagonal_and_far_enemy_attack_noop },
    { TEST_NAME("game_turn_order_compacts_when_non_active_entity_dies_during_attack"), test_game_turn_order_compacts_when_non_active_entity_dies_during_attack },
    { TEST_NAME("game_entity_pressed_adjacent_enemy_attacks_then_noops_when_ap_zero"), test_game_entity_pressed_adjacent_enemy_attacks_then_noops_when_ap_zero },
    { TEST_NAME("game_ranged_attack_hits_at_max_range_without_moving"), test_game_ranged_attack_hits_at_max_range_without_moving },
    { TEST_NAME("game_ranged_attack_noop_beyond_range"), test_game_ranged_attack_noop_beyond_range },
    { TEST_NAME("game_ranged_attack_blocked_by_unit_in_path"), test_game_ranged_attack_blocked_by_unit_in_path },
    { TEST_NAME("game_attack_toggle_after_move_selection_does_not_overflow_scratch"), test_game_attack_toggle_after_move_selection_does_not_overflow_scratch },
    { TEST_NAME("game_selecting_shows_reachable_tiles_and_toggle_shows_attack_range_tiles"), test_game_selecting_shows_reachable_tiles_and_toggle_shows_attack_range_tiles },
};

const uint32_t g_game_combat_tests_count = sizeof(g_game_combat_tests) / sizeof(g_game_combat_tests[0]);
