#include "scenario.h"
#include "skill.h"

PUBLIC game_state_t scenario_setup_default(linear_allocator_t* allocator, int grid_width, int grid_height, int fb_width, int fb_height, int hud_height) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, grid_width, grid_height);
    grid_set_walkable(grid, (position_t){7, 4}, false);
    grid_set_walkable(grid, (position_t){7, 5}, false);

    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    // Every entity gets both skills (ticket 007 / PLAN.md Q11): the
    // entity_spawn skill becomes the default selected_skill, the
    // entity_add_skill call gives it the other option too. Symmetric across
    // both teams so the AI's multi-skill logic (ticket 005) and the
    // player's skill-selection UI (ticket 006) both get exercised by the
    // shipped scenario, not just by dedicated unit tests. No principled
    // "correct" squad composition exists for this -- flagged as a scope
    // call in FLAGGED_DECISIONS.md, not derived from any gameplay
    // requirement.
    entity_t *p1 = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){1, 2}, 10, 1, 3, SKILL_RANGED);
    entity_add_skill(p1, SKILL_MELEE);
    entity_t *p2 = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){1, 5}, 10, 1, 3, SKILL_MELEE);
    entity_add_skill(p2, SKILL_RANGED);
    entity_t *p3 = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){1, 8}, 10, 1, 3, SKILL_MELEE);
    entity_add_skill(p3, SKILL_RANGED);

    entity_t *e1 = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){14, 2}, 10, 1, 3, SKILL_RANGED);
    entity_add_skill(e1, SKILL_MELEE);
    entity_t *e2 = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){14, 5}, 10, 1, 3, SKILL_MELEE);
    entity_add_skill(e2, SKILL_RANGED);
    entity_t *e3 = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){14, 8}, 10, 1, 3, SKILL_MELEE);
    entity_add_skill(e3, SKILL_RANGED);

    // Turn order is authored here, the same way the roster above is: one
    // call per entity, in the exact sequence it should act.
    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p1);
    turn_order_add(allocator, &order, e1);
    turn_order_add(allocator, &order, p2);
    turn_order_add(allocator, &order, e2);
    turn_order_add(allocator, &order, p3);
    turn_order_add(allocator, &order, e3);

    return game_init(allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, fb_width, fb_height, hud_height);
}
