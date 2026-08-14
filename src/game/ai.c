#include "ai.h"

#include "action.h"
#include "pathing.h"
#include "skill.h"

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
    pathing_state_t pathing = pathing_compute_distances(allocator, grid, entities, enemy, enemy->position, max_steps);

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

PRIVATE bool ai_step_toward(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t* enemy, entity_t *target) {
    int max_steps = grid.width * grid.height;
    // We compute the distance from the target.
    // The smallest distance of ennemy neighbor is the tile we are going to move towards.
    pathing_state_t pathing = pathing_compute_distances(allocator, grid, entities, target, target->position, max_steps);

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

    if (!found) {
        return false;
    }

    return action_try_move(allocator, grid, entities, enemy, best_position);
}

// Runs one enemy's turn:
// - find nearest ALIVE player entity via BFS rooted at the enemy's tile (skip_entity=enemy)
// - if already within skill range and ap>0, action_try_attack
// - otherwise, while mp>0 and out of range: BFS rooted at the TARGET's tile
//   (skip_entity=ENTITY_ID_NONE) for a distance-to-target field, step to the
//   orthogonal neighbor (order: up, right, down, left) with smallest
//   distance-to-target via action_try_move, one tile at a time, then recheck
// - no-op if no alive player entities remain
// Returns the attacked entity, or 0 if no attack landed.
PUBLIC entity_t* ai_run_ennemy_turn(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t *enemy) {
    entity_t* target = ai_find_nearest_player(allocator, grid, entities, enemy);
    if (target == 0) {
        return 0;
    }

    if (action_try_attack(allocator, grid, entities, enemy, target)) {
        return target;
    }

    while (enemy->mp > 0 && !skill_target_in_range(allocator, grid, entities, enemy, target)) {
        if (!ai_step_toward(allocator, grid, entities, enemy, target)) {
            break;
        }
    }

    return action_try_attack(allocator, grid, entities, enemy, target) ? target : 0;
}
