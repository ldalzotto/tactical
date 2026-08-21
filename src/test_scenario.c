#include "test_scenario.h"
#include "lib/assert.h"
#include "game/scenario.h"
#include "game/skill.h"
#include "test_game_helpers.h"

PRIVATE void test_scenario_setup_default_populates_map_and_units(linear_allocator_t *allocator) {
    game_state_t game = scenario_setup_default(allocator, GAME_TEST_GRID_WIDTH, GAME_TEST_GRID_HEIGHT, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    assert_test(SLICE_TYPESIZE(game.entities) == 6);

    struct {
        int x, y;
        entity_team_t team;
        skill_t default_skill; // skills[0]
        skill_t other_skill;   // skills[1]
    } expected[6] = {
        // p1 carries SKILL_FIREBALL instead of SKILL_MELEE (see
        // scenario_setup_default) so the AoE feature is reachable
        // end-to-end, within VIEWPORT_MAX_SKILL_BUTTONS's 2-button cap.
        { 1, 2, ENTITY_TEAM_PLAYER, SKILL_RANGED, SKILL_FIREBALL },
        { 1, 5, ENTITY_TEAM_PLAYER, SKILL_MELEE, SKILL_RANGED },
        { 1, 8, ENTITY_TEAM_PLAYER, SKILL_MELEE, SKILL_RANGED },
        { 14, 2, ENTITY_TEAM_ENEMY, SKILL_RANGED, SKILL_MELEE },
        { 14, 5, ENTITY_TEAM_ENEMY, SKILL_MELEE, SKILL_RANGED },
        { 14, 8, ENTITY_TEAM_ENEMY, SKILL_MELEE, SKILL_RANGED },
    };

    for (int id = 0; id < 6; id++) {
        entity_t *entity = &SLICE_AT(game.entities, id);
        assert_test(entity->position.x == expected[id].x);
        assert_test(entity->position.y == expected[id].y);
        assert_test(entity->team == expected[id].team);
        assert_test(entity->hp == 10);
        assert_test(entity->max_hp == 10);
        assert_test(entity->ap == 1);
        assert_test(entity->max_ap == 1);
        assert_test(entity->mp == 3);
        assert_test(entity->max_mp == 3);
        assert_test(entity->alive);

        // Every entity in the default scenario now has both skills.
        assert_test(entity_skill_count(entity) == 2);
        assert_test(SLICE_AT(entity->skills, 0).range == expected[id].default_skill.range);
        assert_test(SLICE_AT(entity->skills, 0).damage == expected[id].default_skill.damage);
        assert_test(SLICE_AT(entity->skills, 0).ap_cost == expected[id].default_skill.ap_cost);
        assert_test(SLICE_AT(entity->skills, 1).range == expected[id].other_skill.range);
        assert_test(SLICE_AT(entity->skills, 1).damage == expected[id].other_skill.damage);
        assert_test(SLICE_AT(entity->skills, 1).ap_cost == expected[id].other_skill.ap_cost);
        assert_test(SLICE_AT(entity->skills, 1).aoe_radius == expected[id].other_skill.aoe_radius);
    }

    assert_test(grid_tile_kind(game.grid, (position_t){7, 4}) == TILE_WALL);
    assert_test(grid_tile_kind(game.grid, (position_t){7, 5}) == TILE_WALL);
    assert_test(grid_tile_kind(game.grid, (position_t){7, 6}) == TILE_GRASS);
    assert_test(grid_tile_kind(game.grid, (position_t){7, 1}) == TILE_CHASM);
    assert_test(grid_tile_kind(game.grid, (position_t){7, 2}) == TILE_CHASM);
    assert_test(!grid_blocks_sight(game.grid, (position_t){7, 1}));
    assert_test(!grid_blocks_sight(game.grid, (position_t){7, 2}));

    for (int y = 0; y < game.grid.height; y++) {
        for (int x = 0; x < game.grid.width; x++) {
            bool is_wall = (x == 7 && (y == 4 || y == 5));
            bool is_chasm = (x == 7 && (y == 1 || y == 2));
            if (is_wall || is_chasm) {
                continue;
            }
            assert_test(grid_is_walkable(game.grid, (position_t){x, y}));
        }
    }

    assert_test(entity_alive_count(game.entities, ENTITY_TEAM_PLAYER) == 3);
    assert_test(entity_alive_count(game.entities, ENTITY_TEAM_ENEMY) == 3);

    // Turn order alternates player/enemy: p1, e1, p2, e2, p3, e3.
    int expected_order_ids[6] = { 0, 3, 1, 4, 2, 5 };
    assert_test(SLICE_TYPESIZE(game.turn.order) == 6);
    for (int i = 0; i < 6; i++) {
        assert_test(SLICE_AT(game.turn.order, i) == &SLICE_AT(game.entities, expected_order_ids[i]));
    }
    assert_test(turn_active_entity(game.turn) == &SLICE_AT(game.entities, 0));

    game_deinit(allocator, game);
}

// grid_set_tile/grid_tile_kind round-trip for all four archetypes.
PRIVATE void test_grid_set_tile_round_trips_all_archetypes(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 1);

    grid_set_tile(grid, (position_t){0, 0}, TILE_FLOOR);
    grid_set_tile(grid, (position_t){1, 0}, TILE_WALL);
    grid_set_tile(grid, (position_t){2, 0}, TILE_GRASS);
    grid_set_tile(grid, (position_t){3, 0}, TILE_CHASM);

    assert_test(grid_tile_kind(grid, (position_t){0, 0}) == TILE_FLOOR);
    assert_test(grid_is_walkable(grid, (position_t){0, 0}));
    assert_test(!grid_blocks_sight(grid, (position_t){0, 0}));

    assert_test(grid_tile_kind(grid, (position_t){1, 0}) == TILE_WALL);
    assert_test(!grid_is_walkable(grid, (position_t){1, 0}));
    assert_test(grid_blocks_sight(grid, (position_t){1, 0}));

    assert_test(grid_tile_kind(grid, (position_t){2, 0}) == TILE_GRASS);
    assert_test(grid_is_walkable(grid, (position_t){2, 0}));
    assert_test(grid_blocks_sight(grid, (position_t){2, 0}));

    assert_test(grid_tile_kind(grid, (position_t){3, 0}) == TILE_CHASM);
    assert_test(!grid_is_walkable(grid, (position_t){3, 0}));
    assert_test(!grid_blocks_sight(grid, (position_t){3, 0}));

    grid_deinit(allocator, grid);
    linear_allocator_pop(allocator, grid_padding);
}

const test_case_t g_scenario_tests[] = {
    { TEST_NAME("scenario_setup_default_populates_map_and_units"), test_scenario_setup_default_populates_map_and_units },
    { TEST_NAME("grid_set_tile_round_trips_all_archetypes"), test_grid_set_tile_round_trips_all_archetypes },
};

const uint32_t g_scenario_tests_count = sizeof(g_scenario_tests) / sizeof(g_scenario_tests[0]);
