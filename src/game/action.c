#include "action.h"
#include "pathing.h"
#include "skill.h"

#include "../lib/assert.h"

PRIVATE bool position_in_tiles(slice_position_t tiles, position_t position) {
    for (SLICE_FOREACH(tiles, tile_s)) {
        if (position_equals(SLICE_DEREF(tile_s), position)) {
            return true;
        }
    }
    return false;
}

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

PUBLIC bool action_try_attack(entity_t* attacker, skill_t skill, entity_t* defender, slice_position_t attack_range_tiles) {
    assert_debug(attacker->alive);
    assert_debug(defender->alive);

    if (attacker->team == defender->team) {
        return false;
    }

    if (attacker->ap < skill.ap_cost) {
        return false;
    }

    if (!position_in_tiles(attack_range_tiles, defender->position)) {
        return false;
    }

    attacker->ap -= skill.ap_cost;
    entity_damage(defender, skill.damage);

    return true;
}

PUBLIC bool action_try_attack_area(linear_allocator_t *allocator, slice_entity_t entities, entity_t *attacker, skill_t skill, position_t impact, slice_position_t attack_range_tiles, slice_position_t blast_tiles, slice_entity_ptr_t *out_hit) {
    assert_debug(attacker->alive);
    assert_debug(skill.aoe_radius > 0);
    // The only caller (game_cast_attack_area) already validated impact via
    // skill_can_target_area, the same check attack_range_tiles is built
    // from -- see action.h's doc comment.
    assert_debug(position_in_tiles(attack_range_tiles, impact));

    if (attacker->ap < skill.ap_cost) {
        return false;
    }

    attacker->ap -= skill.ap_cost;

    // Caller must have `allocator`'s cursor pre-aligned to
    // _Alignof(entity_ptr_t) with `blast_tiles` already staged below it --
    // this function does not self-align (see entity_list_align et al. for
    // the push-align-then-push convention).
    slice_entity_ptr_t hit;
    hit = LINEAR_ALLOCATOR_PUSH(allocator, hit, 0);

    for (SLICE_FOREACH(entities, entity_s)) {
        entity_t *entity = &SLICE_DEREF(entity_s);
        if (!entity->alive || entity->team == attacker->team) {
            continue;
        }

        if (!position_in_tiles(blast_tiles, entity->position)) {
            continue;
        }

        entity_damage(entity, skill.damage);

        slice_entity_ptr_t entry = LINEAR_ALLOCATOR_PUSH_GROW(allocator, &hit, 1);
        SLICE_DEREF(entry) = entity;
    }

    *out_hit = hit;

    return true;
}
