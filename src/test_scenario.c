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
        skill_t default_skill; // entity_spawn's skill, i.e. skills[0]
        skill_t other_skill;   // added via entity_add_skill, i.e. skills[1]
    } expected[6] = {
        { 1, 2, ENTITY_TEAM_PLAYER, SKILL_RANGED, SKILL_MELEE },
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
        assert_test(entity->hp == 10 && entity->max_hp == 10);
        assert_test(entity->ap == 1 && entity->max_ap == 1);
        assert_test(entity->mp == 3 && entity->max_mp == 3);
        assert_test(entity->alive);

        // Every entity in the default scenario now has both skills, with
        // entity_spawn's original single skill still at skills[0].
        assert_test(entity->skill_count == 2);
        assert_test(entity->skills[0].range == expected[id].default_skill.range);
        assert_test(entity->skills[0].damage == expected[id].default_skill.damage);
        assert_test(entity->skills[0].ap_cost == expected[id].default_skill.ap_cost);
        assert_test(entity->skills[1].range == expected[id].other_skill.range);
        assert_test(entity->skills[1].damage == expected[id].other_skill.damage);
        assert_test(entity->skills[1].ap_cost == expected[id].other_skill.ap_cost);
    }

    assert_test(!grid_is_walkable(game.grid, (position_t){7, 4}));
    assert_test(!grid_is_walkable(game.grid, (position_t){7, 5}));

    for (int y = 0; y < game.grid.height; y++) {
        for (int x = 0; x < game.grid.width; x++) {
            if ((x == 7 && y == 4) || (x == 7 && y == 5)) {
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

const test_case_t g_scenario_tests[] = {
    { TEST_NAME("scenario_setup_default_populates_map_and_units"), test_scenario_setup_default_populates_map_and_units },
};

const uint32_t g_scenario_tests_count = sizeof(g_scenario_tests) / sizeof(g_scenario_tests[0]);
