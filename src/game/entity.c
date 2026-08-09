#include "entity.h"

#include "../lib/assert.h"

entity_list_t entity_list_init(linear_allocator_t *allocator) {
    slice_entity_t entities;
    entities = LINEAR_ALLOCATOR_PUSH(allocator, entities, 0);

    entity_list_t list = { .entities = entities };
    return list;
}

void entity_list_deinit(linear_allocator_t *allocator, entity_list_t list) {
    LINEAR_ALLOCATOR_POP(allocator, list.entities);
}

entity_t* entity_spawn(linear_allocator_t *allocator, entity_list_t *list, entity_team_t team, int x, int y, int hp, int ap, int mp) {
    // We are allowed to push an entity only at the same time where the list is created. For now.
    assert_debug(allocator->cursor == list->entities.end);

    slice_entity_t entity_s;
    entity_s = LINEAR_ALLOCATOR_PUSH(allocator, entity_s, 1);
    
    SLICE_DEREF(entity_s) = (entity_t){
        .x = x,
        .y = y,
        .team = team,
        .hp = hp,
        .max_hp = hp,
        .ap = ap,
        .max_ap = ap,
        .mp = mp,
        .max_mp = mp,
        .alive = true,
    };

    list->entities.end = entity_s.end;

    return &SLICE_DEREF(entity_s);
}

entity_t *entity_find_at(entity_list_t list, int x, int y) {
    for (SLICE_FOREACH(list.entities, entity_s)) {
        entity_t *entity = &SLICE_DEREF(entity_s);
        if (entity->alive && entity->x == x && entity->y == y) {
            return entity;
        }
    }

    return 0;
}

void entity_damage(entity_t* entity, int amount) {
    entity->hp -= amount;
    if (entity->hp <= 0) {
        entity->hp = 0;
        entity->alive = false;
    }
}

bool entity_is_adjacent(entity_t a, entity_t b) {
    int dx = a.x - b.x;
    if (dx < 0) dx = -dx;
    int dy = a.y - b.y;
    if (dy < 0) dy = -dy;

    return (dx + dy) == 1;
}

int entity_alive_count(entity_list_t list, entity_team_t team) {
    int count = 0;
    for ( SLICE_FOREACH(list.entities, entity_s) ) {
        entity_t *entity = &SLICE_DEREF(entity_s);
        if (entity->alive && entity->team == team) {
            count++;
        }
    }

    return count;
}
