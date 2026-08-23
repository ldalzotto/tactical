#include "action.h"
#include "pathing.h"
#include "skill.h"

#include "../lib/assert.h"

PUBLIC bool action_try_move(pathing_state_t walking_distances, grid_t grid, entity_t* entity, position_t target) {
    int distance = pathing_distance_at(walking_distances, grid, target);

    if (distance < 0) {
        return false;
    }
    // walking_distances is BFS'd capped at entity->mp by its caller, so a
    // reachable tile can never have a distance greater than the mover's
    // remaining mp.
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

PUBLIC bool action_try_attack_area(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t *attacker, skill_t skill, position_t impact, slice_position_t cached_blast_tiles, bool cached_blast_valid, slice_entity_ptr_t *out_hit) {
    assert_debug(attacker->alive);
    assert_debug(skill.aoe_radius > 0);

    if (attacker->ap < skill.ap_cost) {
        return false;
    }

    if (!skill_area_in_range(grid, entities, attacker, skill, impact)) {
        return false;
    }

    attacker->ap -= skill.ap_cost;

    // Caller must have `allocator`'s cursor aligned to _Alignof(entity_ptr_t)
    // before calling (matching this codebase's push-align-then-push
    // convention for every other typed list -- see entity_list_align et al.);
    // this function does not self-align. When not reusing the cache,
    // blast_tiles is staged first from that aligned cursor; sizeof(position_t)
    // is a multiple of _Alignof(entity_ptr_t), so blast_tiles.end stays just
    // as aligned, and hit can be grown one entity_ptr_t at a time right
    // above it. When reusing the cache, blast_tiles lives in game->scratch
    // instead (not staged on `allocator` at all), so hit is pushed directly
    // at the caller's aligned cursor.
    slice_position_t blast_tiles = cached_blast_valid
        ? cached_blast_tiles
        : pathing_compute_blast_tiles(allocator, grid, impact, skill.aoe_radius);

    slice_entity_ptr_t hit;
    hit = LINEAR_ALLOCATOR_PUSH(allocator, hit, 0);

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

        slice_entity_ptr_t entry = LINEAR_ALLOCATOR_PUSH_GROW(allocator, &hit, 1);
        SLICE_DEREF(entry) = entity;
    }

    if (cached_blast_valid) {
        // blast_tiles lives in game->scratch (owned by the pathing cache),
        // not staged on `allocator` -- hit already sits directly at the
        // caller's aligned cursor with nothing beneath it on `allocator` to
        // reclaim, so it's already final. Reusing linear_allocator_pop_move
        // here (as in the branch below) would try to compact hit onto
        // blast_tiles.begin, corrupting game->pathing.blast_preview_tiles.
        *out_hit = hit;
    } else {
        // hit now sits above blast_tiles, which is no longer needed. Move hit
        // down onto blast_tiles' space and pop the allocator back to just the
        // hits, reclaiming blast_tiles in the same step.
        *out_hit = (slice_entity_ptr_t){ .slice = linear_allocator_pop_move(allocator, hit.slice, blast_tiles.begin) };
    }

    return true;
}
