#pragma once

#include "../lib/linkage.h"

#include <stdbool.h>
#include <stdint.h>

#include "../lib/memory.h"
#include "position.h"

typedef enum {
    ENTITY_TEAM_PLAYER = 0,
    ENTITY_TEAM_ENEMY = 1,
} entity_team_t;

typedef struct {
    int range;
    int damage;
    int ap_cost;
} skill_t;

SLICE_DEFINE(skill_t);

typedef struct {
    position_t position;
    entity_team_t team;
    int hp, max_hp, ap, max_ap, mp, max_mp;
    bool alive;
    // Sub-range of a shared skill_list (see skill_list_add), not owned here.
    slice_skill_t skills;
} entity_t;

SLICE_DEFINE(entity_t);

PUBLIC slice_entity_t entity_list_init(linear_allocator_t *allocator);
PUBLIC void entity_list_deinit(linear_allocator_t *allocator, slice_entity_t list);
// Spawns with skills empty; assign entity->skills after populating them via skill_list_add.
PUBLIC entity_t* entity_spawn(linear_allocator_t *allocator, slice_entity_t* list, entity_team_t team, position_t position, int hp, int ap, int mp);
PUBLIC int entity_skill_count(entity_t *entity);
// Shared, contiguous list every entity's `skills` is a sub-range of.
PUBLIC slice_skill_t skill_list_init(linear_allocator_t *allocator);
PUBLIC void skill_list_deinit(linear_allocator_t *allocator, slice_skill_t list);
PUBLIC skill_t* skill_list_add(linear_allocator_t *allocator, slice_skill_t *list, skill_t skill);
PUBLIC entity_t *entity_find_at(slice_entity_t list, position_t position);
PUBLIC void entity_damage(entity_t* entity, int amount);
PUBLIC int entity_alive_count(slice_entity_t list, entity_team_t team);

#ifdef APP_UNITY_BUILD
#include "entity.c"
#endif
