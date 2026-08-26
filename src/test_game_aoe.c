#include "test_game_aoe.h"
#include "lib/assert.h"
#include "game/entity.h"
#include "game/skill.h"
#include "game/grid.h"
#include "game/turn.h"
#include "test_game_helpers.h"

// F1-07: functional AoE tests -- click-to-cast, damage application, turn
// order reconciliation, game over. Driven exclusively through the public
// game API (game_on_input_event via test_game_helpers.h's click wrappers),
// matching this codebase's convention for testing action/pathing behavior
// (see test_game_combat.c/test_game_movement.c). Hover-preview behavior is
// F1-08, in this same file/suite.

// Blast center (3,1), aoe_radius 2. e_in_a and e_in_b sit at the near/far
// edge of that radius (off the attacker's own approach row, so neither
// blocks the attacker's line of sight to the impact tile the way an entity
// standing on that ray would -- see test_game_combat.c's
// ranged_attack_blocked_by_enemy_in_path) and take damage; e_out sits one
// tile past the edge and is untouched.
PRIVATE void test_aoe_blast_hits_all_enemies_in_radius_and_spares_beyond(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 8, 3);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 1}, 10, 1, 3);
    entity_t* e_in_a = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 0}, 10, 1, 3);
    entity_t* e_in_b = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){5, 1}, 10, 1, 3);
    entity_t* e_out = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){6, 1}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_FIREBALL);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *e_in_a_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e_in_a->skills = (slice_skill_t){ .begin = e_in_a_skills_begin, .end = skills.end };
    skill_t *e_in_b_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e_in_b->skills = (slice_skill_t){ .begin = e_in_b_skills_begin, .end = skills.end };
    skill_t *e_out_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e_out->skills = (slice_skill_t){ .begin = e_out_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, e_in_a);
    turn_order_add(allocator, &order, e_in_b);
    turn_order_add(allocator, &order, e_out);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);
    test_click_tile(&game, allocator, (position_t){3, 1});

    assert_test(e_in_a->hp == 10 - SKILL_FIREBALL.damage);
    assert_test(e_in_b->hp == 10 - SKILL_FIREBALL.damage);
    assert_test(e_out->hp == 10);
    assert_test(p->ap == 1 - SKILL_FIREBALL.ap_cost);

    game_deinit(allocator, game);
}

// A wall directly between the blast center and an enemy within Manhattan
// radius does not shadow it: the blast footprint is pure Manhattan-radius,
// with no line-of-sight component, so both the enemy behind the wall and
// another enemy at the same radius with a clear sightline take damage.
PRIVATE void test_aoe_blast_ignores_obstacles(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 5, 5);
    grid_set_tile(grid, (position_t){2, 3}, TILE_WALL);

    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 2}, 10, 1, 3);
    entity_t* e_behind_wall = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){2, 4}, 10, 1, 3);
    entity_t* e_clear = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){4, 2}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_FIREBALL);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *e_behind_wall_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e_behind_wall->skills = (slice_skill_t){ .begin = e_behind_wall_skills_begin, .end = skills.end };
    skill_t *e_clear_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e_clear->skills = (slice_skill_t){ .begin = e_clear_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, e_behind_wall);
    turn_order_add(allocator, &order, e_clear);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);
    // Impact center (2,2): the wall at (2,3) sits directly between it and
    // e_behind_wall at (2,4), both within aoe_radius (2) of the center.
    test_click_tile(&game, allocator, (position_t){2, 2});

    assert_test(e_behind_wall->hp == 10 - SKILL_FIREBALL.damage);
    assert_test(e_clear->hp == 10 - SKILL_FIREBALL.damage);
    assert_test(p->ap == 1 - SKILL_FIREBALL.ap_cost);

    game_deinit(allocator, game);
}

// Casting on an ally's tile (entity-click AoE dispatch, per F1-04) puts
// both the ally and the attacker itself inside the blast radius: neither
// takes damage, since AoE damage always excludes the attacker's own team.
// An enemy at the edge of the same radius still takes damage.
PRIVATE void test_aoe_no_friendly_fire_spares_ally_and_attacker(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 6, 5);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){2, 2}, 10, 1, 3);
    entity_t* ally = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){3, 2}, 10, 1, 3);
    entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){5, 2}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_FIREBALL);
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
    // ally occupies (3,2): this click routes through game_on_entity_pressed,
    // not game_on_tile_pressed -- the entity-click AoE path.
    test_click_tile(&game, allocator, ally->position);

    assert_test(ally->hp == 10);
    // p's own tile (2,2) is within aoe_radius (2) of the impact (3,2), yet
    // p itself takes no damage either.
    assert_test(p->hp == 10);
    assert_test(e->hp == 10 - SKILL_FIREBALL.damage);
    assert_test(p->ap == 1 - SKILL_FIREBALL.ap_cost);

    game_deinit(allocator, game);
}

// AP is spent exactly once per successful cast, regardless of how many
// entities the blast hits (3, here).
PRIVATE void test_aoe_ap_spent_exactly_once_regardless_of_hit_count(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 8, 3);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 1}, 10, 3, 3);
    // None of these sit on the attacker's straight-line approach to the
    // impact tile (3,1) -- see the sibling test above for why that matters.
    entity_t* e1 = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 0}, 10, 1, 3);
    entity_t* e2 = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 2}, 10, 1, 3);
    entity_t* e3 = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){5, 1}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_FIREBALL);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *e1_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e1->skills = (slice_skill_t){ .begin = e1_skills_begin, .end = skills.end };
    skill_t *e2_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e2->skills = (slice_skill_t){ .begin = e2_skills_begin, .end = skills.end };
    skill_t *e3_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e3->skills = (slice_skill_t){ .begin = e3_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, e1);
    turn_order_add(allocator, &order, e2);
    turn_order_add(allocator, &order, e3);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);
    test_click_tile(&game, allocator, (position_t){3, 1});

    assert_test(e1->hp == 10 - SKILL_FIREBALL.damage);
    assert_test(e2->hp == 10 - SKILL_FIREBALL.damage);
    assert_test(e3->hp == 10 - SKILL_FIREBALL.damage);
    assert_test(p->ap == 3 - SKILL_FIREBALL.ap_cost);

    game_deinit(allocator, game);
}

// Two ways an AoE cast can legally fail: the impact tile is beyond
// skill.range (Manhattan distance), or it's within range but LOS-blocked
// by a wall. Both spend no AP and damage nothing -- action_try_attack_area
// returning false, per F1-03.
PRIVATE void test_aoe_out_of_range_and_los_blocked_impact_fails_cleanly(linear_allocator_t *allocator) {
    // Out of range: impact at distance 6, beyond SKILL_FIREBALL.range (4).
    {
        slice_t grid_padding = grid_align(allocator);
        grid_t grid = grid_init(allocator, 10, 1);
        slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
        slice_entity_t entities = entity_list_init(allocator);
        entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 1, 3);
        entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){7, 0}, 10, 1, 3);

        slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
        slice_skill_t skills = skill_list_init(allocator);
        skill_t *p_skills_begin = skills.end;
        skill_list_add(allocator, &skills, SKILL_FIREBALL);
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
        test_click_tile(&game, allocator, (position_t){6, 0});

        assert_test(p->ap == 1);
        assert_test(e->hp == 10);
        assert_test(game.mode == GAME_MODE_ATTACK);

        game_deinit(allocator, game);
    }

    // LOS-blocked: impact at distance 4 (within range), but a wall sits on
    // the straight ray from the attacker. The impact tile is occupied by e,
    // so this click also exercises the entity-click AoE path failing.
    {
        slice_t grid_padding = grid_align(allocator);
        grid_t grid = grid_init(allocator, 5, 3);
        grid_set_tile(grid, (position_t){2, 1}, TILE_WALL);

        slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
        slice_entity_t entities = entity_list_init(allocator);
        entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 1}, 10, 1, 3);
        entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){4, 1}, 10, 1, 3);

        slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
        slice_skill_t skills = skill_list_init(allocator);
        skill_t *p_skills_begin = skills.end;
        skill_list_add(allocator, &skills, SKILL_FIREBALL);
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
        test_click_tile(&game, allocator, e->position);

        assert_test(p->ap == 1);
        assert_test(e->hp == 10);
        assert_test(game.mode == GAME_MODE_ATTACK);

        game_deinit(allocator, game);
    }
}

// Empty grass (walkable but sight-blocking) in range is not a legal AoE
// impact: nothing there to see or target, so both a click-to-cast and a
// hover preview must reject it, same as an out-of-range or LOS-blocked tile.
PRIVATE void test_aoe_empty_sight_blocking_tile_rejected_for_cast_and_preview(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 5, 1);
    grid_set_tile(grid, (position_t){2, 0}, TILE_GRASS);

    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_FIREBALL);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);

    test_move_tile(&game, allocator, (position_t){2, 0});
    assert_test(SLICE_TYPESIZE(game.pathing.blast_tiles) == 0);

    test_click_tile(&game, allocator, (position_t){2, 0});
    assert_test(p->ap == 1);
    assert_test(game.mode == GAME_MODE_ATTACK);

    game_deinit(allocator, game);
}

// A blast that kills two of three enemies in one cast removes both from
// game.turn.order, leaves the cursor pointing at the still-active attacker,
// and leaves every remaining entry alive.
PRIVATE void test_aoe_multi_kill_reconciles_turn_order(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 8, 3);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 1}, 10, 1, 3);
    // Off the attacker's straight-line approach to the impact tile (3,1) --
    // see test_aoe_blast_hits_all_enemies_in_radius_and_spares_beyond.
    entity_t* e1 = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 0}, 3, 1, 3);
    entity_t* e2 = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 2}, 3, 1, 3);
    // Beyond aoe_radius (2) of the impact (3,1): survives untouched.
    entity_t* e3 = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){6, 1}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_FIREBALL);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *e1_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e1->skills = (slice_skill_t){ .begin = e1_skills_begin, .end = skills.end };
    skill_t *e2_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e2->skills = (slice_skill_t){ .begin = e2_skills_begin, .end = skills.end };
    skill_t *e3_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e3->skills = (slice_skill_t){ .begin = e3_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, e1);
    turn_order_add(allocator, &order, e2);
    turn_order_add(allocator, &order, e3);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);
    test_click_tile(&game, allocator, (position_t){3, 1});

    assert_test(!e1->alive);
    assert_test(!e2->alive);
    assert_test(e3->alive);
    assert_test(SLICE_TYPESIZE(game.turn.order) == 2);
    assert_test(SLICE_AT(game.turn.order, 0) == p);
    assert_test(SLICE_AT(game.turn.order, 1) == e3);
    assert_test(turn_active_entity(game.turn) == p);
    assert_test(game.game_over == GAME_OVER_NONE);

    game_deinit(allocator, game);
}

// A blast that kills every remaining enemy in one cast (two, here) sets
// game.game_over to GAME_OVER_WIN exactly once -- game_check_game_over
// asserts its precondition on entry, so a double call would itself panic
// under assert_debug.
PRIVATE void test_aoe_multi_kill_wipes_enemy_team_triggers_game_over(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 8, 3);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 1}, 10, 1, 3);
    // Off the attacker's straight-line approach to the impact tile (3,1) --
    // see test_aoe_blast_hits_all_enemies_in_radius_and_spares_beyond.
    entity_t* e1 = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 0}, 3, 1, 3);
    entity_t* e2 = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 2}, 3, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_FIREBALL);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *e1_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e1->skills = (slice_skill_t){ .begin = e1_skills_begin, .end = skills.end };
    skill_t *e2_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e2->skills = (slice_skill_t){ .begin = e2_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, e1);
    turn_order_add(allocator, &order, e2);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);
    test_click_tile(&game, allocator, (position_t){3, 1});

    assert_test(!e1->alive);
    assert_test(!e2->alive);
    assert_test(game.game_over == GAME_OVER_WIN);
    assert_test(SLICE_TYPESIZE(game.turn.order) == 1);
    assert_test(turn_active_entity(game.turn) == p);

    game_deinit(allocator, game);
}

// Casting with no entity on the impact tile itself still works and hits
// whichever entities are in the surrounding radius -- the tile-click AoE
// dispatch path (game_on_tile_pressed), distinct from the entity-click
// cases exercised above.
PRIVATE void test_aoe_cast_onto_empty_tile_hits_surrounding_entities(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 6, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 1, 3);
    entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 0}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_FIREBALL);
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
    // (2,0) is empty -- e sits one tile further out, within aoe_radius (2).
    assert_test(entity_find_at(game.entities, (position_t){2, 0}) == 0);
    test_click_tile(&game, allocator, (position_t){2, 0});

    assert_test(e->hp == 10 - SKILL_FIREBALL.damage);
    assert_test(p->ap == 1 - SKILL_FIREBALL.ap_cost);
    assert_test(p->hp == 10);
    // A successful AoE cast closes attack mode automatically, same as
    // single-target attacks.
    assert_test(game.mode == GAME_MODE_MOVEMENT);

    game_deinit(allocator, game);
}

// Insufficient AP fails an AoE cast the same way it fails a single-target
// one (action_try_attack_area's ap_cost check, mirroring action_try_attack).
// Hovers the impact tile first so the cast reuses the staged blast preview
// instead of computing fresh, exercising game_cast_attack_area's
// cached-preview unwind path on failure.
PRIVATE void test_aoe_insufficient_ap_fails_cleanly(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 6, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    // ap = 0 (also becomes max_ap, so turn_init's reset leaves it at 0).
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 0, 3);
    entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 0}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_FIREBALL);
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
    test_move_tile(&game, allocator, (position_t){2, 0});

    assert_test(SLICE_TYPESIZE(game.pathing.blast_tiles) > 0);

    test_click_tile(&game, allocator, (position_t){2, 0});

    assert_test(p->ap == 0);
    assert_test(e->hp == 10);
    assert_test(game.mode == GAME_MODE_ATTACK);

    game_deinit(allocator, game);
}

// A corpse (dead but still present in game.entities -- a live entity's
// position only frees up once it dies, entity_find_at already skips it via
// its alive check) sitting inside a later blast radius takes no further
// damage: action_try_attack_area's entity loop skips already-dead entities
// the same way it skips same-team ones.
PRIVATE void test_aoe_ignores_already_dead_entities_within_radius(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 6, 3);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 1}, 10, 2, 3);
    // Killed by a melee attack below, before the AoE cast -- its corpse
    // then sits within the later AoE blast radius.
    entity_t* corpse = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 1}, 1, 1, 3);
    entity_t* e_live = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){5, 1}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    skill_list_add(allocator, &skills, SKILL_FIREBALL);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *corpse_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    corpse->skills = (slice_skill_t){ .begin = corpse_skills_begin, .end = skills.end };
    skill_t *e_live_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e_live->skills = (slice_skill_t){ .begin = e_live_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, corpse);
    turn_order_add(allocator, &order, e_live);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);
    // skills[0] (SKILL_MELEE) is selected by default: kills the adjacent
    // corpse-to-be outright (hp 1, damage 5).
    test_click_tile(&game, allocator, corpse->position);
    assert_test(!corpse->alive);
    assert_test(p->ap == 1);

    // Switch to SKILL_FIREBALL and cast centered on (3,1): corpse (1,1) is
    // within aoe_radius (2) of the center but already dead; e_live (5,1) is
    // also within radius and alive, so it takes damage normally.
    test_click_attack_toggle(&game, allocator);
    test_click_skill_button(&game, allocator, 1);
    test_click_tile(&game, allocator, (position_t){3, 1});

    assert_test(corpse->hp == 0);
    assert_test(!corpse->alive);
    assert_test(e_live->hp == 10 - SKILL_FIREBALL.damage);
    assert_test(p->ap == 1 - SKILL_FIREBALL.ap_cost);

    game_deinit(allocator, game);
}

// F1-08: live blast preview tests, extending this same suite (shared
// SKILL_FIREBALL/attack-mode-selection setup boilerplate with F1-07 above).
// Driven via test_move_tile/test_move_to_pixel (MOUSE_MOVE) and
// test_tile_list_contains against game.pathing.blast_tiles (F1-05).

// Hovering a valid AoE impact tile populates blast_tiles with
// exactly the Manhattan-radius blast footprint pathing_compute_blast_tiles
// would return for that tile, obstacles included -- same wall setup as
// test_aoe_blast_ignores_obstacles, but checked via hover instead of cast.
PRIVATE void test_aoe_preview_ignores_obstacles(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 5, 5);
    grid_set_tile(grid, (position_t){2, 3}, TILE_WALL);

    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 2}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_list_add(allocator, &skills, SKILL_FIREBALL);
    p->skills = skills;

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);
    // Impact center (2,2): the wall at (2,3) sits between it and (2,4) but
    // no longer shadows anything, same as test_aoe_blast_ignores_obstacles.
    test_move_tile(&game, allocator, (position_t){2, 2});

    assert_test(test_tile_list_contains(game.pathing.blast_tiles, (position_t){2, 2}));
    assert_test(test_tile_list_contains(game.pathing.blast_tiles, (position_t){4, 2}));
    assert_test(test_tile_list_contains(game.pathing.blast_tiles, (position_t){2, 4}));

    game_deinit(allocator, game);
}

// Moving hover from one valid impact tile to another recomputes the
// preview to match the new tile exactly -- not a stale union of both.
PRIVATE void test_aoe_preview_updates_as_hover_moves(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 8, 3);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 1}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_list_add(allocator, &skills, SKILL_FIREBALL);
    p->skills = skills;

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);

    test_move_tile(&game, allocator, (position_t){3, 1});
    assert_test(test_tile_list_contains(game.pathing.blast_tiles, (position_t){5, 1}));

    test_move_tile(&game, allocator, (position_t){2, 1});
    // (5,1) was in range of the (3,1) center (distance 2) but is outside
    // aoe_radius (2) of the new (2,1) center (distance 3): a stale union
    // would still contain it.
    assert_test(!test_tile_list_contains(game.pathing.blast_tiles, (position_t){5, 1}));
    assert_test(test_tile_list_contains(game.pathing.blast_tiles, (position_t){2, 1}));

    game_deinit(allocator, game);
}

// Hover leaving the grid (hover_valid becomes false) empties the preview.
PRIVATE void test_aoe_preview_clears_when_hover_leaves_grid(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 8, 3);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 1}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_list_add(allocator, &skills, SKILL_FIREBALL);
    p->skills = skills;

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);
    test_move_tile(&game, allocator, (position_t){3, 1});
    assert_test(SLICE_TYPESIZE(game.pathing.blast_tiles) > 0);

    // Well outside the viewport in every direction -- screen_to_grid fails,
    // hover_valid becomes false.
    test_move_to_pixel(&game, allocator, -100, -100);
    assert_test(!game.hover_valid);
    assert_test(SLICE_TYPESIZE(game.pathing.blast_tiles) == 0);

    game_deinit(allocator, game);
}

// Toggling out of GAME_MODE_ATTACK empties the preview, matching
// pathing_ranges_reset's existing clear-on-mode-change behavior.
PRIVATE void test_aoe_preview_clears_on_mode_change(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 8, 3);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 1}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_list_add(allocator, &skills, SKILL_FIREBALL);
    p->skills = skills;

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);
    test_move_tile(&game, allocator, (position_t){3, 1});
    assert_test(SLICE_TYPESIZE(game.pathing.blast_tiles) > 0);

    test_click_attack_toggle(&game, allocator);
    assert_test(game.mode == GAME_MODE_MOVEMENT);
    assert_test(SLICE_TYPESIZE(game.pathing.blast_tiles) == 0);

    game_deinit(allocator, game);
}

// Switching from an AoE skill to a non-AoE one (SKILL_MELEE) empties an
// already-populated preview.
PRIVATE void test_aoe_preview_clears_on_skill_switch_to_non_aoe(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 8, 3);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 1}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    // skills[0] = SKILL_FIREBALL (AoE, selected by default), skills[1] =
    // SKILL_MELEE (not AoE).
    skill_list_add(allocator, &skills, SKILL_FIREBALL);
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = skills;

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);
    test_move_tile(&game, allocator, (position_t){3, 1});
    assert_test(SLICE_TYPESIZE(game.pathing.blast_tiles) > 0);

    test_click_skill_button(&game, allocator, 1);
    assert_test(game.selected_skill == 1);
    assert_test(SLICE_TYPESIZE(game.pathing.blast_tiles) == 0);

    game_deinit(allocator, game);
}

// Hovering a tile beyond the attacker's skill.range leaves the preview
// empty -- no partial/incorrect preview for an uncastable target.
PRIVATE void test_aoe_preview_clears_for_out_of_range_hover(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 10, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_list_add(allocator, &skills, SKILL_FIREBALL);
    p->skills = skills;

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);
    // Distance 6, beyond SKILL_FIREBALL.range (4).
    test_move_tile(&game, allocator, (position_t){6, 0});

    assert_test(game.hover_valid);
    assert_test(SLICE_TYPESIZE(game.pathing.blast_tiles) == 0);

    game_deinit(allocator, game);
}

// While a preview is populated, attack_range_tiles remains populated and
// correct -- the two overlays coexist (F1-05), not mutually exclusive.
PRIVATE void test_aoe_preview_coexists_with_attack_range(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 8, 3);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 1}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_list_add(allocator, &skills, SKILL_FIREBALL);
    p->skills = skills;

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);
    test_move_tile(&game, allocator, (position_t){3, 1});

    assert_test(SLICE_TYPESIZE(game.pathing.blast_tiles) > 0);
    assert_test(SLICE_TYPESIZE(game.pathing.attack_range_tiles) > 0);
    // The hovered impact tile itself is a legal attack-range target too.
    assert_test(test_tile_list_contains(game.pathing.attack_range_tiles, (position_t){3, 1}));

    game_deinit(allocator, game);
}

// Hovering (and the preview computation it triggers) never mutates
// entity/turn state -- purely visual, same guarantee game_set_mode already
// gives walking_distances/attack_range_tiles.
PRIVATE void test_aoe_preview_computation_is_read_only(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 8, 3);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 1}, 10, 1, 3);
    entity_t* e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 0}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_FIREBALL);
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

    position_t p_position_before = p->position;
    int p_ap_before = p->ap, p_hp_before = p->hp, p_mp_before = p->mp;
    position_t e_position_before = e->position;
    int e_ap_before = e->ap, e_hp_before = e->hp, e_mp_before = e->mp;

    test_move_tile(&game, allocator, (position_t){3, 1});
    assert_test(SLICE_TYPESIZE(game.pathing.blast_tiles) > 0);

    assert_test(position_equals(p->position, p_position_before));
    assert_test(p->ap == p_ap_before);
    assert_test(p->hp == p_hp_before);
    assert_test(p->mp == p_mp_before);
    assert_test(position_equals(e->position, e_position_before));
    assert_test(e->ap == e_ap_before);
    assert_test(e->hp == e_hp_before);
    assert_test(e->mp == e_mp_before);

    game_deinit(allocator, game);
}

// Re-selecting the already-active AoE skill via its skill button (not a
// mouse move) while hovering a valid, in-range impact tile recomputes the
// preview through game_on_skill_button_pressed's own eligibility check --
// distinct from the mouse-move path exercised by the tests above.
PRIVATE void test_aoe_reselecting_skill_button_stages_preview_for_in_range_hover(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 8, 3);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 1}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    // A second skill is required for the skill-button row to be visible at
    // all -- layout_visible_skill_button_count returns 0 for skill_count <=
    // 1, in which case a click here would never even dispatch to
    // game_on_skill_button_pressed.
    skill_list_add(allocator, &skills, SKILL_FIREBALL);
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = skills;

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);
    test_move_tile(&game, allocator, (position_t){3, 1});
    assert_test(SLICE_TYPESIZE(game.pathing.blast_tiles) > 0);

    // Re-clicking skill button 0 re-selects SKILL_FIREBALL (already active)
    // while hover is still valid and in range: game_on_skill_button_pressed
    // must restage the preview itself, not just rely on the prior hover.
    test_click_skill_button(&game, allocator, 0);

    assert_test(SLICE_TYPESIZE(game.pathing.blast_tiles) > 0);
    assert_test(test_tile_list_contains(game.pathing.blast_tiles, (position_t){3, 1}));

    game_deinit(allocator, game);
}

// Same skill-button re-selection path, but the hovered tile is out of the
// skill's range: game_on_skill_button_pressed's attack_range_tiles
// membership check must fail closed, leaving the preview empty.
PRIVATE void test_aoe_reselecting_skill_button_leaves_preview_empty_for_out_of_range_hover(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 10, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    // A second skill is required for the skill-button row to be visible at
    // all -- see the sibling in-range test above.
    skill_list_add(allocator, &skills, SKILL_FIREBALL);
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = skills;

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);
    // Distance 6, beyond SKILL_FIREBALL.range (4): hover_valid is true (the
    // tile is on the grid) but it's outside attack_range_tiles.
    test_move_tile(&game, allocator, (position_t){6, 0});
    assert_test(game.hover_valid);
    assert_test(SLICE_TYPESIZE(game.pathing.blast_tiles) == 0);

    test_click_skill_button(&game, allocator, 0);

    assert_test(SLICE_TYPESIZE(game.pathing.blast_tiles) == 0);

    game_deinit(allocator, game);
}

// Clicking a skill button while still in GAME_MODE_MOVEMENT (attack mode
// never toggled on) must not stage a preview, regardless of hover state --
// game_on_skill_button_pressed's mode check has to gate this independently
// of skill_is_aoe/attack_range_tiles membership.
PRIVATE void test_aoe_skill_button_in_movement_mode_does_not_stage_preview(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 8, 3);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 1}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    // A second skill is required for the skill-button row to be visible at
    // all -- see test_aoe_reselecting_skill_button_stages_preview_for_in_range_hover.
    skill_list_add(allocator, &skills, SKILL_FIREBALL);
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = skills;

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    assert_test(game.mode == GAME_MODE_MOVEMENT);
    test_move_tile(&game, allocator, (position_t){3, 1});
    assert_test(game.hover_valid);

    // Skill buttons are visible whenever mode != GAME_MODE_NONE (and
    // skill_count > 1), so this is clickable in movement mode even though
    // attack was never toggled on.
    test_click_skill_button(&game, allocator, 0);

    assert_test(game.mode == GAME_MODE_MOVEMENT);
    assert_test(SLICE_TYPESIZE(game.pathing.blast_tiles) == 0);

    game_deinit(allocator, game);
}

// Hovering over a valid, in-range tile in attack mode while a non-AoE skill
// is selected must not stage a preview -- the mouse-move handler's
// skill_is_aoe check has to reject a melee skill, not just accept fireballs
// (every other hover test in this suite only ever hovers with an AoE
// skill selected).
PRIVATE void test_aoe_hover_with_non_aoe_skill_selected_stages_no_preview(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 8, 3);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 1}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    // skills[0] = SKILL_MELEE (not AoE, selected by default), skills[1] =
    // SKILL_FIREBALL (AoE).
    skill_list_add(allocator, &skills, SKILL_MELEE);
    skill_list_add(allocator, &skills, SKILL_FIREBALL);
    p->skills = skills;

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);
    assert_test(game.selected_skill == 0);

    // (3,1) is within SKILL_FIREBALL's range/radius of the attacker, but
    // SKILL_MELEE is what's selected here.
    test_move_tile(&game, allocator, (position_t){3, 1});

    assert_test(game.hover_valid);
    assert_test(SLICE_TYPESIZE(game.pathing.blast_tiles) == 0);

    game_deinit(allocator, game);
}

// Casting at the currently-hovered impact tile resolves correctly --
// game_on_entity_pressed/game_on_tile_pressed restage blast_tiles for the
// clicked tile unconditionally (see
// test_aoe_cast_at_different_tile_than_hover_resolves_correctly for the case
// where that tile differs from the current hover), so game_cast_attack_area
// itself never computes blast footprints, only reads what its caller just
// staged.
PRIVATE void test_aoe_cast_at_hovered_tile_resolves_correctly(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 8, 3);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 1}, 10, 1, 3);
    entity_t* e_in_a = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 0}, 10, 1, 3);
    entity_t* e_in_b = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){5, 1}, 10, 1, 3);
    entity_t* e_out = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){6, 1}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_FIREBALL);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *e_in_a_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e_in_a->skills = (slice_skill_t){ .begin = e_in_a_skills_begin, .end = skills.end };
    skill_t *e_in_b_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e_in_b->skills = (slice_skill_t){ .begin = e_in_b_skills_begin, .end = skills.end };
    skill_t *e_out_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e_out->skills = (slice_skill_t){ .begin = e_out_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, e_in_a);
    turn_order_add(allocator, &order, e_in_b);
    turn_order_add(allocator, &order, e_out);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);
    test_move_tile(&game, allocator, (position_t){3, 1});

    assert_test(SLICE_TYPESIZE(game.pathing.blast_tiles) > 0);
    assert_test(test_tile_list_contains(game.pathing.blast_tiles, (position_t){3, 0}));
    assert_test(test_tile_list_contains(game.pathing.blast_tiles, (position_t){5, 1}));

    // Cast at the exact tile just hovered -- reuses the staged preview.
    test_click_tile(&game, allocator, (position_t){3, 1});

    assert_test(e_in_a->hp == 10 - SKILL_FIREBALL.damage);
    assert_test(e_in_b->hp == 10 - SKILL_FIREBALL.damage);
    assert_test(e_out->hp == 10);
    assert_test(p->ap == 1 - SKILL_FIREBALL.ap_cost);

    game_deinit(allocator, game);
}

// Entering ATTACK mode with no valid hover yet (mouse never moved onto the
// grid) must leave blast_tiles empty, not stage it against garbage/stale
// hover data -- the counterpart to
// test_aoe_cast_at_hovered_tile_resolves_correctly's in-range case, covering
// game_set_mode's `!game->hover_valid` branch.
PRIVATE void test_aoe_entering_attack_mode_without_hover_stages_no_blast(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 8, 3);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 1}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_list_add(allocator, &skills, SKILL_FIREBALL);
    p->skills = skills;

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    // Move off the grid entirely -- hover_valid is false when attack mode
    // is toggled on below.
    test_move_to_pixel(&game, allocator, -100, -100);
    assert_test(!game.hover_valid);

    test_click_attack_toggle(&game, allocator);

    assert_test(game.mode == GAME_MODE_ATTACK);
    assert_test(SLICE_TYPESIZE(game.pathing.blast_tiles) == 0);

    game_deinit(allocator, game);
}

// Casting with no blast_tiles staged at all (empty -- see
// test_aoe_entering_attack_mode_without_hover_stages_no_blast for how that
// state is reached) still resolves correctly: game_on_tile_pressed stages
// blast_tiles for its impact unconditionally, regardless of whatever state
// it was in before the click.
PRIVATE void test_aoe_cast_with_no_staged_blast_computes_fresh(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 8, 3);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 1}, 10, 1, 3);
    entity_t* e_in_a = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 0}, 10, 1, 3);
    entity_t* e_in_b = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){5, 1}, 10, 1, 3);
    entity_t* e_out = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){6, 1}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_FIREBALL);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *e_in_a_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e_in_a->skills = (slice_skill_t){ .begin = e_in_a_skills_begin, .end = skills.end };
    skill_t *e_in_b_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e_in_b->skills = (slice_skill_t){ .begin = e_in_b_skills_begin, .end = skills.end };
    skill_t *e_out_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e_out->skills = (slice_skill_t){ .begin = e_out_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, e_in_a);
    turn_order_add(allocator, &order, e_in_b);
    turn_order_add(allocator, &order, e_out);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    // Move off the grid, then toggle attack -- blast_tiles stays empty (see
    // test_aoe_entering_attack_mode_without_hover_stages_no_blast).
    test_move_to_pixel(&game, allocator, -100, -100);
    test_click_attack_toggle(&game, allocator);
    assert_test(SLICE_TYPESIZE(game.pathing.blast_tiles) == 0);

    // Raw MOUSE_CLICK at an in-range tile -- no MOUSE_MOVE ever staged
    // blast_tiles for it, or anywhere else.
    int px, py;
    grid_to_screen(game.viewport, 3, 1, &px, &py);
    input_event_t click = { .type = INPUT_EVENT_MOUSE_CLICK, .x = px + 1, .y = py + 1 };
    game_on_input_event(&game, allocator, click);

    assert_test(e_in_a->hp == 10 - SKILL_FIREBALL.damage);
    assert_test(e_in_b->hp == 10 - SKILL_FIREBALL.damage);
    assert_test(e_out->hp == 10);
    assert_test(p->ap == 1 - SKILL_FIREBALL.ap_cost);

    game_deinit(allocator, game);
}

// A MOUSE_CLICK's impact tile isn't guaranteed to be the last MOUSE_MOVE's
// hover tile -- test_click_tile's helper always hovers its own target
// first, so it can't produce this by itself; this test bypasses it with a
// raw click to hover a *different* valid impact tile than the one it
// clicks. game_on_tile_pressed restages blast_tiles fresh for its own
// impact regardless of what was last hovered, so the cast still resolves
// correctly against the clicked tile, not the stale hovered one.
PRIVATE void test_aoe_cast_at_different_tile_than_hover_resolves_correctly(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 8, 3);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 1}, 10, 1, 3);
    entity_t* e_in_a = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 0}, 10, 1, 3);
    entity_t* e_in_b = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){5, 1}, 10, 1, 3);
    entity_t* e_out = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){6, 1}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_FIREBALL);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *e_in_a_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e_in_a->skills = (slice_skill_t){ .begin = e_in_a_skills_begin, .end = skills.end };
    skill_t *e_in_b_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e_in_b->skills = (slice_skill_t){ .begin = e_in_b_skills_begin, .end = skills.end };
    skill_t *e_out_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e_out->skills = (slice_skill_t){ .begin = e_out_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, e_in_a);
    turn_order_add(allocator, &order, e_in_b);
    turn_order_add(allocator, &order, e_out);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);
    // Hover a different, still-valid impact tile than the one clicked below
    // (centered on (2,1), a fireball wouldn't reach e_in_b at (5,1) -- if the
    // click below wrongly reused this stale footprint instead of restaging
    // for its own impact, the damage assertions after it would catch that).
    test_move_tile(&game, allocator, (position_t){2, 1});
    assert_test(SLICE_TYPESIZE(game.pathing.blast_tiles) > 0);

    // Raw MOUSE_CLICK at (3,1) -- also in range, but not the tile blast_tiles
    // was staged for.
    int px, py;
    grid_to_screen(game.viewport, 3, 1, &px, &py);
    input_event_t click = { .type = INPUT_EVENT_MOUSE_CLICK, .x = px + 1, .y = py + 1 };
    game_on_input_event(&game, allocator, click);

    assert_test(e_in_a->hp == 10 - SKILL_FIREBALL.damage);
    assert_test(e_in_b->hp == 10 - SKILL_FIREBALL.damage);
    assert_test(e_out->hp == 10);
    assert_test(p->ap == 1 - SKILL_FIREBALL.ap_cost);

    game_deinit(allocator, game);
}

// End-to-end: blast_tiles (also what the overlay draws) and
// action_try_attack_area's hit set (what execution damages) are the same
// footprint, checked generically over every opposing-team entity rather
// than a hand-picked pair.
PRIVATE void test_aoe_blast_tiles_match_what_execution_hits(linear_allocator_t *allocator) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 8, 3);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t* p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 1}, 10, 1, 3);
    entity_t* e_in_a = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 0}, 10, 1, 3);
    entity_t* e_in_b = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){5, 1}, 10, 1, 3);
    entity_t* e_out = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){6, 1}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_FIREBALL);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *e_in_a_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e_in_a->skills = (slice_skill_t){ .begin = e_in_a_skills_begin, .end = skills.end };
    skill_t *e_in_b_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e_in_b->skills = (slice_skill_t){ .begin = e_in_b_skills_begin, .end = skills.end };
    skill_t *e_out_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e_out->skills = (slice_skill_t){ .begin = e_out_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, e_in_a);
    turn_order_add(allocator, &order, e_in_b);
    turn_order_add(allocator, &order, e_out);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, 320, 240, 40);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);
    test_move_tile(&game, allocator, (position_t){3, 1});
    assert_test(SLICE_TYPESIZE(game.pathing.blast_tiles) > 0);

    // Capture the previewed footprint before casting mutates game state.
    position_t footprint[64];
    int footprint_count = 0;
    for (SLICE_FOREACH(game.pathing.blast_tiles, tile_s)) {
        assert_test(footprint_count < 64);
        footprint[footprint_count] = SLICE_DEREF(tile_s);
        footprint_count++;
    }
    assert_test(footprint_count > 0);

    test_click_tile(&game, allocator, (position_t){3, 1});

    for (SLICE_FOREACH(game.entities, entity_s)) {
        entity_t *entity = &SLICE_DEREF(entity_s);
        if (entity->team != ENTITY_TEAM_ENEMY) {
            continue;
        }

        bool in_footprint = false;
        for (int i = 0; i < footprint_count; i++) {
            if (position_equals(footprint[i], entity->position)) {
                in_footprint = true;
                break;
            }
        }

        if (in_footprint) {
            assert_test(entity->hp == 10 - SKILL_FIREBALL.damage);
        } else {
            assert_test(entity->hp == 10);
        }
    }

    game_deinit(allocator, game);
}

const test_case_t g_game_aoe_tests[] = {
    { TEST_NAME("aoe_blast_hits_all_enemies_in_radius_and_spares_beyond"), test_aoe_blast_hits_all_enemies_in_radius_and_spares_beyond },
    { TEST_NAME("aoe_blast_ignores_obstacles"), test_aoe_blast_ignores_obstacles },
    { TEST_NAME("aoe_no_friendly_fire_spares_ally_and_attacker"), test_aoe_no_friendly_fire_spares_ally_and_attacker },
    { TEST_NAME("aoe_ap_spent_exactly_once_regardless_of_hit_count"), test_aoe_ap_spent_exactly_once_regardless_of_hit_count },
    { TEST_NAME("aoe_out_of_range_and_los_blocked_impact_fails_cleanly"), test_aoe_out_of_range_and_los_blocked_impact_fails_cleanly },
    { TEST_NAME("aoe_empty_sight_blocking_tile_rejected_for_cast_and_preview"), test_aoe_empty_sight_blocking_tile_rejected_for_cast_and_preview },
    { TEST_NAME("aoe_multi_kill_reconciles_turn_order"), test_aoe_multi_kill_reconciles_turn_order },
    { TEST_NAME("aoe_multi_kill_wipes_enemy_team_triggers_game_over"), test_aoe_multi_kill_wipes_enemy_team_triggers_game_over },
    { TEST_NAME("aoe_cast_onto_empty_tile_hits_surrounding_entities"), test_aoe_cast_onto_empty_tile_hits_surrounding_entities },
    { TEST_NAME("aoe_insufficient_ap_fails_cleanly"), test_aoe_insufficient_ap_fails_cleanly },
    { TEST_NAME("aoe_ignores_already_dead_entities_within_radius"), test_aoe_ignores_already_dead_entities_within_radius },
    { TEST_NAME("aoe_preview_ignores_obstacles"), test_aoe_preview_ignores_obstacles },
    { TEST_NAME("aoe_preview_updates_as_hover_moves"), test_aoe_preview_updates_as_hover_moves },
    { TEST_NAME("aoe_preview_clears_when_hover_leaves_grid"), test_aoe_preview_clears_when_hover_leaves_grid },
    { TEST_NAME("aoe_preview_clears_on_mode_change"), test_aoe_preview_clears_on_mode_change },
    { TEST_NAME("aoe_preview_clears_on_skill_switch_to_non_aoe"), test_aoe_preview_clears_on_skill_switch_to_non_aoe },
    { TEST_NAME("aoe_preview_clears_for_out_of_range_hover"), test_aoe_preview_clears_for_out_of_range_hover },
    { TEST_NAME("aoe_preview_coexists_with_attack_range"), test_aoe_preview_coexists_with_attack_range },
    { TEST_NAME("aoe_preview_computation_is_read_only"), test_aoe_preview_computation_is_read_only },
    { TEST_NAME("aoe_reselecting_skill_button_stages_preview_for_in_range_hover"), test_aoe_reselecting_skill_button_stages_preview_for_in_range_hover },
    { TEST_NAME("aoe_reselecting_skill_button_leaves_preview_empty_for_out_of_range_hover"), test_aoe_reselecting_skill_button_leaves_preview_empty_for_out_of_range_hover },
    { TEST_NAME("aoe_skill_button_in_movement_mode_does_not_stage_preview"), test_aoe_skill_button_in_movement_mode_does_not_stage_preview },
    { TEST_NAME("aoe_hover_with_non_aoe_skill_selected_stages_no_preview"), test_aoe_hover_with_non_aoe_skill_selected_stages_no_preview },
    { TEST_NAME("aoe_cast_at_hovered_tile_resolves_correctly"), test_aoe_cast_at_hovered_tile_resolves_correctly },
    { TEST_NAME("aoe_entering_attack_mode_without_hover_stages_no_blast"), test_aoe_entering_attack_mode_without_hover_stages_no_blast },
    { TEST_NAME("aoe_cast_with_no_staged_blast_computes_fresh"), test_aoe_cast_with_no_staged_blast_computes_fresh },
    { TEST_NAME("aoe_cast_at_different_tile_than_hover_resolves_correctly"), test_aoe_cast_at_different_tile_than_hover_resolves_correctly },
    { TEST_NAME("aoe_blast_tiles_match_what_execution_hits"), test_aoe_blast_tiles_match_what_execution_hits },
};

const uint32_t g_game_aoe_tests_count = sizeof(g_game_aoe_tests) / sizeof(g_game_aoe_tests[0]);
