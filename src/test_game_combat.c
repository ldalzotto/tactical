#include "test_game_combat.h"
#include "lib/assert.h"
#include "game/entity.h"
#include "game/grid.h"
#include "game/turn.h"
#include "test_game_helpers.h"

PRIVATE void test_game_attack_kills_defender_clamps_hp_and_frees_tile_for_movement(void) {
    static char buffer[4096];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_t grid_padding = grid_align(&allocator);
    grid_t grid = grid_init(&allocator, 4, 4);
    slice_t entity_list_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(&allocator);
    entity_t* p = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 5);
    entity_t* e = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 3, 2, 3);
    entity_t* e2 = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 3}, 10, 2, 3);

    slice_t turn_order_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(&allocator);
    turn_order_add(&allocator, &order, p);
    turn_order_add(&allocator, &order, e);
    turn_order_add(&allocator, &order, e2);

    game_state_t game = game_init(&allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, &allocator, p->position);
    test_click_tile(&game, &allocator, e->position);

    // Attack damage (5) exceeds e's hp (3): hp clamps to 0, not negative.
    assert_test(e->hp == 0);
    assert_test(!e->alive);
    assert_test(p->ap == 1);
    // e2 is still alive, so the enemy team isn't wiped out yet.
    assert_test(game.game_over == GAME_OVER_NONE);

    // e's corpse no longer occupies its tile: pressing it now falls through
    // to a move, same as any other empty tile.
    test_click_tile(&game, &allocator, (position_t){1, 0});

    entity_t *entity = p;
    assert_test(entity->position.x == 1 && entity->position.y == 0);
    assert_test(entity->mp == 4);

    game_deinit(&allocator, game);
}

PRIVATE void test_game_entity_pressed_diagonal_and_far_enemy_attack_noop(void) {
    static char buffer[4096];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_t grid_padding = grid_align(&allocator);
    grid_t grid = grid_init(&allocator, 6, 6);
    slice_t entity_list_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(&allocator);
    entity_t* p = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* diagonal = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 1}, 10, 2, 3);
    entity_t* far = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){5, 5}, 10, 2, 3);

    slice_t turn_order_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(&allocator);
    turn_order_add(&allocator, &order, p);
    turn_order_add(&allocator, &order, diagonal);
    turn_order_add(&allocator, &order, far);

    game_state_t game = game_init(&allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, &allocator, p->position);
    assert_test(game.selected_entity == p);

    // Diagonal doesn't count as adjacent: pressing it is a no-op.
    test_click_tile(&game, &allocator, diagonal->position);
    assert_test(p->ap == 2);
    assert_test(diagonal->hp == 10);

    // Neither does simply being far away.
    test_click_tile(&game, &allocator, far->position);
    assert_test(p->ap == 2);
    assert_test(far->hp == 10);

    game_deinit(&allocator, game);
}

PRIVATE void test_game_turn_order_compacts_when_non_active_entity_dies_during_attack(void) {
    static char buffer[4096];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_t grid_padding = grid_align(&allocator);
    grid_t grid = grid_init(&allocator, 4, 4);
    slice_t entity_list_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(&allocator);
    entity_t* p = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t* b = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 3}, 10, 2, 3);
    entity_t* c = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 3, 2, 3);

    slice_t turn_order_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(&allocator);
    turn_order_add(&allocator, &order, p);
    turn_order_add(&allocator, &order, b);
    turn_order_add(&allocator, &order, c);

    game_state_t game = game_init(&allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, &allocator, p->position);
    test_click_tile(&game, &allocator, c->position);

    assert_test(!c->alive);
    assert_test(SLICE_TYPESIZE(game.turn.order) == 2);
    assert_test(SLICE_AT(game.turn.order, 0) == p);
    assert_test(SLICE_AT(game.turn.order, 1) == b);
    assert_test(turn_active_entity(game.turn) == p);

    game_deinit(&allocator, game);
}

PRIVATE void test_game_entity_pressed_adjacent_enemy_attacks_then_noops_when_ap_zero(void) {
    static char buffer[8192];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_t grid_padding = grid_align(&allocator);
    grid_t grid = grid_init(&allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT);
    slice_t entity_list_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(&allocator);
    entity_t* p = entity_spawn(&allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 1, 3);
    entity_t* e = entity_spawn(&allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 10, 2, 3);

    slice_t turn_order_align = linear_allocator_push_alignment(&allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(&allocator);
    turn_order_add(&allocator, &order, p);
    turn_order_add(&allocator, &order, e);

    game_state_t game = game_init(&allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    test_click_tile(&game, &allocator, p->position);
    assert_test(game.selected_entity == p);

    test_click_tile(&game, &allocator, e->position);
    assert_test(p->ap == 0);
    assert_test(e->hp == 5);
    assert_test(e->alive);

    test_click_tile(&game, &allocator, e->position);
    assert_test(p->ap == 0);
    assert_test(e->hp == 5);

    game_deinit(&allocator, game);
}

const test_case_t g_game_combat_tests[] = {
    { TEST_NAME("game_attack_kills_defender_clamps_hp_and_frees_tile_for_movement"), test_game_attack_kills_defender_clamps_hp_and_frees_tile_for_movement },
    { TEST_NAME("game_entity_pressed_diagonal_and_far_enemy_attack_noop"), test_game_entity_pressed_diagonal_and_far_enemy_attack_noop },
    { TEST_NAME("game_turn_order_compacts_when_non_active_entity_dies_during_attack"), test_game_turn_order_compacts_when_non_active_entity_dies_during_attack },
    { TEST_NAME("game_entity_pressed_adjacent_enemy_attacks_then_noops_when_ap_zero"), test_game_entity_pressed_adjacent_enemy_attacks_then_noops_when_ap_zero },
};

const uint32_t g_game_combat_tests_count = sizeof(g_game_combat_tests) / sizeof(g_game_combat_tests[0]);
