#include "ai.h"

#include "action.h"
#include "pathing.h"
#include "skill.h"

#include "../lib/assert.h"

/*
    Distance from the BFS root to a tile adjacent to `position`.
    (`position` itself is unreachable in the field — the candidate
    occupies it — so take the min over its four neighbors instead.
    0 means the candidate is already adjacent to the root.)
*/
PRIVATE int ai_distance_to_adjacency(pathing_state_t pathing, grid_t grid, position_t position) {
    int best = -1;
    for (SLICE_FOREACH(POSITION_DIRECTIONS, dir_s)) {
        position_t dir = SLICE_DEREF(dir_s);
        position_t neighbor = position_add(position, dir);
        if (!grid_in_bounds(grid, neighbor)) {
            continue;
        }
        int d = pathing_distance_at(pathing, grid, neighbor);
        if (d < 0) {
            continue;
        }
        if (best < 0 || d < best) {
            best = d;
        }
    }

    return best;
}

PRIVATE entity_t* ai_find_nearest_player(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t* enemy) {
    int max_steps = grid.width * grid.height;
    pathing_state_t pathing = pathing_compute_walking_distances(allocator, grid, entities, enemy->position, max_steps);

    entity_t* best_entity = 0;
    int best_dist = -1;

    for ( SLICE_FOREACH(entities, candidate_s) ) {
        entity_t *candidate = &SLICE_DEREF(candidate_s);
        if (!candidate->alive || candidate->team != ENTITY_TEAM_PLAYER) {
            continue;
        }

        int dist = ai_distance_to_adjacency(pathing, grid, candidate->position);
        if (dist < 0) {
            continue;
        }

        if (best_entity == 0 || dist < best_dist) {
            best_entity = candidate;
            best_dist = dist;
        }
    }

    pathing_deinit(allocator, pathing);

    return best_entity;
}

PRIVATE void ai_step_toward(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t* enemy, entity_t *target) {
    int max_steps = grid.width * grid.height;
    // We compute the distance from the target.
    // The smallest distance of ennemy neighbor is the tile we are going to move towards.
    pathing_state_t pathing = pathing_compute_walking_distances(allocator, grid, entities, target->position, max_steps);

    bool found = false;
    int best_dist = -1;
    position_t best_position = { 0, 0 };

    for (SLICE_FOREACH(POSITION_DIRECTIONS, dir_s)) {
        position_t dir = SLICE_DEREF(dir_s);
        position_t neighbor = position_add(enemy->position, dir);
        if (!grid_in_bounds(grid, neighbor)) {
            continue;
        }

        int dist = pathing_distance_at(pathing, grid, neighbor);
        if (dist < 0) {
            continue;
        }

        if (!found || dist < best_dist) {
            found = true;
            best_dist = dist;
            best_position = neighbor;
        }
    }

    pathing_deinit(allocator, pathing);

    // ai_run_ennemy_turn only calls us after ai_find_nearest_player found a
    // reachable player, which implies at least one of the enemy's neighbors
    // is reachable from that player (the BFS path is reversible), so `found`
    // is always true here.
    assert_debug(found);

    // action_try_move takes its distance grid from the caller now, so
    // build it here: a fresh BFS rooted at the mover, capped at its own mp.
    pathing_state_t move_distances = pathing_compute_walking_distances(allocator, grid, entities, enemy->position, enemy->mp);
    action_try_move(move_distances, grid, enemy, best_position);
    pathing_deinit(allocator, move_distances);
}

// True if a beats b as the "preferred" skill: higher damage, tie-broken by
// lower ap_cost, then list order (first found wins).
PRIVATE bool ai_skill_beats(skill_t a, skill_t b) {
    if (a.damage != b.damage) {
        return a.damage > b.damage;
    }
    return a.ap_cost < b.ap_cost;
}

// Enemy's strongest skill by damage (see ai_skill_beats) -- what movement
// aims to get in range of, so the AI closes for its best option instead of
// stopping at the first skill in range.
PRIVATE skill_t* ai_preferred_skill(entity_t *enemy) {
    skill_t *best = 0;
    for (SLICE_FOREACH(enemy->skills, skill_s)) {
        skill_t *skill = &SLICE_DEREF(skill_s);
        if (best == 0 || ai_skill_beats(*skill, *best)) {
            best = skill;
        }
    }
    assert_debug(best != 0);
    return best;
}

// Highest-damage skill (see ai_skill_beats) among those currently in range
// of `target`, or 0 if none are in range.
PRIVATE skill_t* ai_best_in_range_skill(grid_t grid, slice_entity_t entities, entity_t *enemy, entity_t *target) {
    skill_t *best = 0;
    for (SLICE_FOREACH(enemy->skills, skill_s)) {
        skill_t *skill = &SLICE_DEREF(skill_s);
        if (!skill_can_target(grid, entities, enemy, *skill, target)) {
            continue;
        }
        if (best == 0 || ai_skill_beats(*skill, *best)) {
            best = skill;
        }
    }
    return best;
}

// AoE counterpart of the plain action_try_attack call below: casts `skill`
// centered on `impact` via action_try_attack_area, then splices any
// resulting casualties into `dead`.
//
// `dead` sits below this call's own temp allocations (blast_tiles, then
// action_try_attack_area's out_hit) on `allocator`'s stack, so it can't be
// grown via ordinary push-and-grow -- the temp regions are in the way. Instead
// this reserves room for the casualties right where `dead` currently ends via
// linear_allocator_insert, sliding the temp regions above it up to make room
// (same technique pathing_ranges_push_tiles uses to grow a persistent region
// while temp data sits on top of it), then copies the dead pointers in.
PRIVATE slice_entity_ptr_t ai_try_attack_area(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t *enemy, skill_t skill, position_t impact) {
    slice_entity_ptr_t dead = LINEAR_ALLOCATOR_PUSH(allocator, dead, 0);

    slice_t blast_align = linear_allocator_push_alignment(allocator, _Alignof(position_t));
    slice_position_t blast_tiles = pathing_compute_blast_tiles(allocator, grid, impact, skill.aoe_radius);

    // `impact` (the target's own tile) was already range/LOS-validated by
    // ai_best_in_range_skill via skill_can_target, so a single-tile range set
    // is enough to satisfy action_try_attack_area's in-range check.
    position_t attack_range_tile[1] = { impact };
    slice_position_t attack_range_tiles = {
        .begin = attack_range_tile,
        .end = typeoffset(attack_range_tile, 1),
    };

    slice_t hit_align = linear_allocator_push_alignment(allocator, _Alignof(entity_ptr_t));
    slice_entity_ptr_t out_hit;
    if (action_try_attack_area(allocator, entities, enemy, skill, impact, attack_range_tiles, blast_tiles, &out_hit)) {
        int dead_count = 0;
        for (SLICE_FOREACH(out_hit, hit_s)) {
            if (!SLICE_DEREF(hit_s)->alive) {
                dead_count++;
            }
        }

        if (dead_count > 0) {
            slice_entity_ptr_t inserted = LINEAR_ALLOCATOR_INSERT(allocator, dead, dead_count);
            ptrdiff_t extra = SLICE_BYTESIZE(inserted);
            out_hit.slice = slice_shift(out_hit.slice, extra);
            hit_align = slice_shift(hit_align, extra);
            blast_tiles.slice = slice_shift(blast_tiles.slice, extra);
            blast_align = slice_shift(blast_align, extra);

            slice_entity_ptr_t write = inserted;
            for (SLICE_FOREACH(out_hit, hit_s)) {
                entity_t *hit = SLICE_DEREF(hit_s);
                if (!hit->alive) {
                    SLICE_DEREF(write) = hit;
                    write = SLICE_ADVANCE(write, 1);
                }
            }
            dead.end = write.begin;
        }

        linear_allocator_pop(allocator, out_hit.slice);
    }
    linear_allocator_pop(allocator, hit_align);
    linear_allocator_pop(allocator, blast_tiles.slice);
    linear_allocator_pop(allocator, blast_align);

    return dead;
}

// Runs one enemy's turn: find the nearest alive player entity, step toward
// it (one tile at a time) until ai_preferred_skill is in range or mp runs
// out, then attack with ai_best_in_range_skill (may be weaker than
// preferred if preferred never came into range).
// Returns every entity killed by the attack; see ai.h.
PUBLIC slice_entity_ptr_t ai_run_ennemy_turn(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t *enemy) {
    slice_entity_ptr_t dead = LINEAR_ALLOCATOR_PUSH(allocator, dead, 0);

    entity_t* target = ai_find_nearest_player(allocator, grid, entities, enemy);
    if (target == 0) {
        return dead;
    }

    skill_t *preferred = ai_preferred_skill(enemy);

    while (enemy->mp > 0 && !skill_can_target(grid, entities, enemy, *preferred, target)) {
        ai_step_toward(allocator, grid, entities, enemy, target);
    }

    skill_t *attack_skill = ai_best_in_range_skill(grid, entities, enemy, target);
    if (attack_skill == 0) {
        return dead;
    }

    if (skill_is_aoe(*attack_skill)) {
        return ai_try_attack_area(allocator, grid, entities, enemy, *attack_skill, target->position);
    }

    // action_try_attack takes range as a tile set now; hand it a
    // single-tile set for the position ai_best_in_range_skill already
    // confirmed in range.
    position_t attack_range_tile[1] = { target->position };
    slice_position_t attack_range_tiles = {
        .begin = attack_range_tile,
        .end = typeoffset(attack_range_tile, 1),
    };

    // dead is still the top of `allocator` here (ai_find_nearest_player
    // and ai_step_toward fully unwind whatever they push), so growing it
    // in place is safe.
    if (action_try_attack(enemy, *attack_skill, target, attack_range_tiles) && !target->alive) {
        slice_entity_ptr_t entry = LINEAR_ALLOCATOR_PUSH_GROW(allocator, &dead, 1);
        SLICE_DEREF(entry) = target;
    }

    return dead;
}
