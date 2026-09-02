#include "entity.h"
#include "game/position.h"
#include "lib/linkage.h"
#include "lib/memory.h"

PUBLIC slice_entity_t entity_list_init(linear_allocator_t *allocator) {
    slice_entity_t entities;
    entities = LINEAR_ALLOCATOR_PUSH(allocator, entities, 0);
    return entities;
}

PUBLIC void entity_list_deinit(linear_allocator_t *allocator, slice_entity_t list) {
    LINEAR_ALLOCATOR_POP(allocator, list);
}

PUBLIC entity_t* entity_spawn(linear_allocator_t *allocator, slice_entity_t *entities, entity_team_t team, position_t position, int hp, int ap, int mp) {
    // For now, entities may only be pushed right after list creation.
    slice_entity_t entry = LINEAR_ALLOCATOR_PUSH_GROW(allocator, entities, 1);

    SLICE_DEREF(entry) = (entity_t){
        .position = position,
        .team = team,
        .hp = hp,
        .max_hp = hp,
        .ap = ap,
        .max_ap = ap,
        .mp = mp,
        .max_mp = mp,
        .alive = true,
        .skills = {0},
    };

    return &SLICE_DEREF(entry);
}

PUBLIC int entity_skill_count(entity_t *entity) {
    return (int)SLICE_TYPESIZE(entity->skills);
}

PUBLIC slice_skill_t skill_list_init(linear_allocator_t *allocator) {
    slice_skill_t list;
    list = LINEAR_ALLOCATOR_PUSH(allocator, list, 0);
    return list;
}

PUBLIC void skill_list_deinit(linear_allocator_t *allocator, slice_skill_t list) {
    LINEAR_ALLOCATOR_POP(allocator, list);
}

PUBLIC skill_t* skill_list_add(linear_allocator_t *allocator, slice_skill_t *list, skill_t skill) {
    // Same append-in-place discipline as entity_spawn/turn_order_add.
    slice_skill_t entry = LINEAR_ALLOCATOR_PUSH_GROW(allocator, list, 1);
    SLICE_DEREF(entry) = skill;
    return &SLICE_DEREF(entry);
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
