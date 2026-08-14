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

// Per-entity loadout cap for this feature (melee + ranged today), not a
// structural constant.
#define ENTITY_MAX_SKILLS 2

typedef struct {
    position_t position;
    entity_team_t team;
    int hp, max_hp, ap, max_ap, mp, max_mp;
    bool alive;
    /*
        TODO: We don't want to have a constant ENTITY_MAX_SKILLS.
        Instead of skills, skill_count. Just use a slice of skill.
        In that sense, the /Users/loicdalzotto/perso/tactical/src/game/entity.h entity_spawn api should be simplified
        to not take skills.
        Because the number of skills can vary.
        When we create an entity, I want the skills to be allocated dynamically with the linear_allocator.
        I would vote to have a separate list of entity skills.

        So that when a scenario is created, you allocate an entity. Then you populate a skills array. And
        the entity references a slice of this array. A little bit like how a database would work.
    */
    skill_t skills[ENTITY_MAX_SKILLS];
    int skill_count;
} entity_t;

SLICE_DEFINE(entity_t);

PUBLIC slice_entity_t entity_list_init(linear_allocator_t *allocator);
PUBLIC void entity_list_deinit(linear_allocator_t *allocator, slice_entity_t list);
// Spawns with exactly one skill (skills[0], skill_count=1).
// Use entity_add_skill afterward to give the entity a second skill.
PUBLIC entity_t* entity_spawn(linear_allocator_t *allocator, slice_entity_t* list, entity_team_t team, position_t position, int hp, int ap, int mp, skill_t skill);
// Appends a skill; asserts skill_count < ENTITY_MAX_SKILLS.
PUBLIC void entity_add_skill(entity_t *entity, skill_t skill);
PUBLIC entity_t *entity_find_at(slice_entity_t list, position_t position);
PUBLIC void entity_damage(entity_t* entity, int amount);
PUBLIC bool entity_is_adjacent(entity_t a, entity_t b);
PUBLIC int entity_alive_count(slice_entity_t list, entity_team_t team);

#ifdef APP_UNITY_BUILD
#include "entity.c"
#endif
