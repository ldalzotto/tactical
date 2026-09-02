#include "test_invariants.h"

#include "game/game.h"
#include "lib/assert.h"
#include "game/entity.h"
#include "game/grid.h"
#include "game/position.h"
#include "lib/linkage.h"
#include "lib/memory.h"

PUBLIC void assert_game_invariants(game_state_t *game) {
    // Everyone in turn order is alive. Not checked the other way (order
    // count == alive count): test fixtures often spawn an entity without
    // adding it to turn order to isolate dispatch behavior.
    for (SLICE_FOREACH(game->turn.order, e_s)) {
        assert_test(SLICE_DEREF(e_s)->alive);
    }

    // Live entities stay in bounds, never share a tile, and hp/ap/mp stay
    // within [0, max] (entity_damage clamps hp at 0; ap/mp only spend down
    // from turn_reset_points()'s max, see action.c).
    for (SLICE_FOREACH(game->entities, a_s)) {
        entity_t *a = &SLICE_DEREF(a_s);

        assert_test(a->hp >= 0);
        assert_test(a->hp <= a->max_hp);
        assert_test(a->ap >= 0);
        assert_test(a->ap <= a->max_ap);
        assert_test(a->mp >= 0);
        assert_test(a->mp <= a->max_mp);

        if (!a->alive) continue;
        assert_test(grid_in_bounds(game->grid, a->position));
        for (SLICE_FOREACH(game->entities, b_s)) {
            entity_t *b = &SLICE_DEREF(b_s);
            if (b == a || !b->alive) continue;
            assert_test(!position_equals(a->position, b->position));
        }
    }

    // walking_distances/attack_range_tiles are mutually exclusive with mode:
    // whichever isn't game->mode's active overlay must be empty.
    if (game->mode == GAME_MODE_ATTACK) {
        assert_test(SLICE_TYPESIZE(game->pathing.walking_distances.dist) == 0);
    } else {
        assert_test(SLICE_TYPESIZE(game->pathing.attack_range_tiles) == 0);
    }

    // game_over, when set, is consistent with alive counts. Not checked the
    // other way: game_check_game_over only runs after combat, so plenty of
    // valid states sit at GAME_OVER_NONE without survivors on both sides.
    if (game->game_over == GAME_OVER_WIN) {
        assert_test(entity_alive_count(game->entities, ENTITY_TEAM_ENEMY) == 0);
    } else if (game->game_over == GAME_OVER_LOSE) {
        assert_test(entity_alive_count(game->entities, ENTITY_TEAM_PLAYER) == 0);
    }
}
