#include "scenario.h"
#include "skill.h"

PUBLIC game_state_t scenario_setup_default(linear_allocator_t* allocator, int grid_width, int grid_height, int fb_width, int fb_height, int hud_height) {
    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, grid_width, grid_height);
    grid_set_walkable(grid, (position_t){7, 4}, false);
    grid_set_walkable(grid, (position_t){7, 5}, false);

    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t *p1 = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){1, 2}, 10, 1, 3);
    entity_t *p2 = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){1, 5}, 10, 1, 3);
    entity_t *p3 = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){1, 8}, 10, 1, 3);
    entity_t *e1 = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){14, 2}, 10, 1, 3);
    entity_t *e2 = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){14, 5}, 10, 1, 3);
    entity_t *e3 = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){14, 8}, 10, 1, 3);

    // Every entity gets both skills, populated into the shared skill list
    // after every entity exists (entity_spawn's own contiguity requirement
    // forbids interleaving anything else in between spawns). Exercises AI
    // skill choice and player skill selection on both teams by default, not
    // just in unit tests. Scope call, not a gameplay requirement -- see
    // FLAGGED_DECISIONS.md.
    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);

    skill_t *p1_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_RANGED);
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p1->skills = (slice_skill_t){ .begin = p1_skills_begin, .end = skills.end };

    skill_t *p2_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    skill_list_add(allocator, &skills, SKILL_RANGED);
    p2->skills = (slice_skill_t){ .begin = p2_skills_begin, .end = skills.end };

    skill_t *p3_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    skill_list_add(allocator, &skills, SKILL_RANGED);
    p3->skills = (slice_skill_t){ .begin = p3_skills_begin, .end = skills.end };

    skill_t *e1_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_RANGED);
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e1->skills = (slice_skill_t){ .begin = e1_skills_begin, .end = skills.end };

    skill_t *e2_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    skill_list_add(allocator, &skills, SKILL_RANGED);
    e2->skills = (slice_skill_t){ .begin = e2_skills_begin, .end = skills.end };

    skill_t *e3_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    skill_list_add(allocator, &skills, SKILL_RANGED);
    e3->skills = (slice_skill_t){ .begin = e3_skills_begin, .end = skills.end };

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

    return game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, fb_width, fb_height, hud_height);
}
