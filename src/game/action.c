#include "action.h"
#include "pathing.h"
#include "skill.h"

#include "../lib/assert.h"

PUBLIC bool action_try_move(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t* entity, position_t target) {
    pathing_state_t pathing = pathing_compute_walking_distances(allocator, grid, entities, entity->position, entity->mp);

    int distance = pathing_distance_at(pathing, grid, target);

    pathing_deinit(allocator, pathing);

    if (distance < 0) {
        return false;
    }
    // pathing_compute_walking_distances caps the BFS at entity->mp, so a reachable
    // tile can never have a distance greater than the mover's remaining mp.
    assert_debug(distance <= entity->mp);

    entity->mp -= distance;
    entity->position = target;

    return true;
}

PUBLIC bool action_try_attack(grid_t grid, slice_entity_t entities, entity_t* attacker, skill_t skill, entity_t* defender) {
    assert_debug(attacker->alive);
    assert_debug(defender->alive);

    if (attacker->team == defender->team) {
        return false;
    }

    if (attacker->ap < skill.ap_cost) {
        return false;
    }

    if (!skill_target_in_range(grid, entities, attacker, skill, defender)) {
        return false;
    }

    attacker->ap -= skill.ap_cost;
    entity_damage(defender, skill.damage);

    return true;
}

PUBLIC bool action_try_attack_area(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t *attacker, skill_t skill, position_t impact, slice_entity_ptr_t *out_hit) {
    assert_debug(attacker->alive);
    assert_debug(skill.aoe_radius > 0);

    if (attacker->ap < skill.ap_cost) {
        return false;
    }

    if (!pathing_in_range(grid, entities, attacker->position, impact, skill.range)) {
        return false;
    }

    attacker->ap -= skill.ap_cost;

    slice_position_t blast_tiles = pathing_compute_blast_tiles(allocator, grid, entities, impact, skill.aoe_radius);

    // Reserve worst-case capacity (every entity hit) up front, above
    // blast_tiles, so out_hit can be filled by direct index instead of
    // growing incrementally -- lets the final trim below pop both the
    // unused reservation tail and blast_tiles in a single call.
    int entity_count = (int)SLICE_TYPESIZE(entities);
    slice_entity_ptr_t hit_capacity;
    hit_capacity = LINEAR_ALLOCATOR_PUSH(allocator, hit_capacity, entity_count);

    int hit_count = 0;
    for (SLICE_FOREACH(entities, entity_s)) {
        entity_t *entity = &SLICE_DEREF(entity_s);
        if (!entity->alive || entity->team == attacker->team) {
            continue;
        }

        bool in_blast = false;
        for (SLICE_FOREACH(blast_tiles, tile_s)) {
            if (position_equals(SLICE_DEREF(tile_s), entity->position)) {
                in_blast = true;
                break;
            }
        }
        if (!in_blast) {
            continue;
        }

        entity_damage(entity, skill.damage);
        SLICE_AT(hit_capacity, hit_count) = entity;
        hit_count++;
    }

    // Trim the unused reservation tail and blast_tiles (sitting above it)
    // in one pop, down to exactly the hit_count entries actually written.
    entity_ptr_t *trimmed_end = hit_capacity.begin + hit_count;
    linear_allocator_pop(allocator, (slice_t){ trimmed_end, allocator->cursor });

    *out_hit = (slice_entity_ptr_t){ .begin = hit_capacity.begin, .end = trimmed_end };

    return true;
}
