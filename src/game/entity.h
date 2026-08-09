#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../lib/memory.h"

typedef enum {
    ENTITY_TEAM_PLAYER = 0,
    ENTITY_TEAM_ENEMY = 1,
} entity_team_t;

typedef struct {
    int x, y;
    entity_team_t team;
    int hp, max_hp, ap, max_ap, mp, max_mp;
    bool alive;
} entity_t;

SLICE_DEFINE(entity_t);

typedef struct {
    slice_entity_t entities;
} entity_list_t;

entity_list_t entity_list_init(linear_allocator_t *allocator);
void entity_list_deinit(linear_allocator_t *allocator, entity_list_t list);
entity_t* entity_spawn(linear_allocator_t *allocator, entity_list_t *list, entity_team_t team, int x, int y, int hp, int ap, int mp);
entity_t *entity_find_at(entity_list_t list, int x, int y);
void entity_damage(entity_t* entity, int amount);
bool entity_is_adjacent(entity_t a, entity_t b);
int entity_alive_count(entity_list_t list, entity_team_t team);
