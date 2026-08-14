#include "entity.h"

#include "../lib/assert.h"

PUBLIC slice_entity_t entity_list_init(linear_allocator_t *allocator) {
    slice_entity_t entities;
    entities = LINEAR_ALLOCATOR_PUSH(allocator, entities, 0);
    return entities;
}

PUBLIC void entity_list_deinit(linear_allocator_t *allocator, slice_entity_t list) {
    LINEAR_ALLOCATOR_POP(allocator, list);
}

PUBLIC entity_t* entity_spawn(linear_allocator_t *allocator, slice_entity_t *entities, entity_team_t team, position_t position, int hp, int ap, int mp, skill_t skill) {
    // We are allowed to push an entity only at the same time where the list is created. For now.
    assert_debug(allocator->cursor == entities->end);

    slice_entity_t entity_s;
    entity_s = LINEAR_ALLOCATOR_PUSH(allocator, entity_s, 1);

    SLICE_DEREF(entity_s) = (entity_t){
        .position = position,
        .team = team,
        .hp = hp,
        .max_hp = hp,
        .ap = ap,
        .max_ap = ap,
        .mp = mp,
        .max_mp = mp,
        .alive = true,
        .skills = { skill },
        .skill_count = 1,
        .selected_skill = 0,
    };

    entities->end = entity_s.end;

    return &SLICE_DEREF(entity_s);
}

PUBLIC void entity_add_skill(entity_t *entity, skill_t skill) {
    assert_debug(entity->skill_count < ENTITY_MAX_SKILLS);
    entity->skills[entity->skill_count] = skill;
    entity->skill_count++;
}

PUBLIC skill_t entity_active_skill(entity_t *entity) {
    assert_debug(entity->selected_skill >= 0 && entity->selected_skill < entity->skill_count);
    return entity->skills[entity->selected_skill];
}

PUBLIC entity_t *entity_find_at(slice_entity_t list, position_t position) {
    for (SLICE_FOREACH(list, entity_s)) {
        entity_t *entity = &SLICE_DEREF(entity_s);
        if (entity->alive && position_equals(entity->position, position)) {
            return entity;
        }
    }

    return 0;
}

PUBLIC void entity_damage(entity_t* entity, int amount) {
    entity->hp -= amount;
    if (entity->hp <= 0) {
        entity->hp = 0;
        entity->alive = false;
    }
}

PUBLIC bool entity_is_adjacent(entity_t a, entity_t b) {
    for (SLICE_FOREACH(POSITION_DIRECTIONS, dir_s)) {
        position_t dir = SLICE_DEREF(dir_s);
        if (position_equals(a.position, position_add(b.position, dir))) {
            return true;
        }
    }
    return false;
}

PUBLIC int entity_alive_count(slice_entity_t list, entity_team_t team) {
    int count = 0;
    for ( SLICE_FOREACH(list, entity_s) ) {
        entity_t *entity = &SLICE_DEREF(entity_s);
        if (entity->alive && entity->team == team) {
            count++;
        }
    }

    return count;
}
