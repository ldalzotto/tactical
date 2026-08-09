#include "entity.h"

#include "../lib/assert.h"

entity_list_t entity_list_init(linear_allocator_t *allocator, int capacity) {
    assert_debug(capacity > 0);

    slice_entity_t entities;
    entities = LINEAR_ALLOCATOR_PUSH(allocator, entities, (size_t)capacity);

    entity_list_t list = { .entities = entities, .count = 0 };
    return list;
}

void entity_list_deinit(linear_allocator_t *allocator, entity_list_t list) {
    LINEAR_ALLOCATOR_POP(allocator, list.entities);
}

entity_id_t entity_spawn(entity_list_t *list, entity_team_t team, int x, int y, int hp, int ap, int mp) {
    int capacity = (int)(list->entities.end - list->entities.begin);
    assert_debug(list->count < capacity);

    entity_id_t id = (entity_id_t)list->count;

    SLICE_AT(list->entities, id) = (entity_t){
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

    list->count++;

    return id;
}

entity_t *entity_at(entity_list_t list, entity_id_t id) {
    assert_debug(id >= 0 && id < list.count);
    return &SLICE_AT(list.entities, id);
}

entity_id_t entity_find_at(entity_list_t list, int x, int y) {
    for (int i = 0; i < list.count; i++) {
        entity_t *entity = &SLICE_AT(list.entities, i);
        if (entity->alive && entity->x == x && entity->y == y) {
            return (entity_id_t)i;
        }
    }

    return ENTITY_ID_NONE;
}

void entity_damage(entity_list_t list, entity_id_t id, int amount) {
    entity_t *entity = entity_at(list, id);

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
    for (int i = 0; i < list.count; i++) {
        entity_t *entity = &SLICE_AT(list.entities, i);
        if (entity->alive && entity->team == team) {
            count++;
        }
    }

    return count;
}
