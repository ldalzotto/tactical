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
    pathing_state_t pathing = pathing_compute_distances(allocator, grid, entities, enemy, enemy->position, max_steps, 0);

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
    pathing_state_t pathing = pathing_compute_distances(allocator, grid, entities, target, target->position, max_steps, 0);

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

// True if a beats b as the "preferred" skill: higher damage, or equal
// damage with a lower ap_cost. Equal on both keeps whichever was found
// first (list order), so index 0 wins ties -- deterministic, no reliance on
// skill_t equality/ordering beyond what the AI actually cares about.
PRIVATE bool ai_skill_beats(skill_t a, skill_t b) {
    if (a.damage != b.damage) {
        return a.damage > b.damage;
    }
    return a.ap_cost < b.ap_cost;
}

// The enemy's strongest skill by damage (ties: lower ap_cost, then list
// order) -- what movement should aim to get into range of, so a multi-skill
// AI closes distance for its best option instead of settling for whichever
// skill happens to be in range first. See ticket 005 / PLAN.md Q9: an
// earlier "stop moving once ANY skill is in range" design made higher-
// damage skills structurally unreachable whenever a longer-range weaker
// skill was available.
PRIVATE int ai_preferred_skill_index(entity_t *enemy) {
    int best = 0;
    for (int i = 1; i < enemy->skill_count; i++) {
        if (ai_skill_beats(enemy->skills[i], enemy->skills[best])) {
            best = i;
        }
    }
    return best;
}

// Among the enemy's skills currently in range of `target`, the one with the
// highest damage (ties: lower ap_cost, then list order), or -1 if none are
// in range. Temporarily mutates enemy->selected_skill while probing each
// skill via skill_target_in_range (which reads entity_active_skill) --
// caller is responsible for setting the final selected_skill afterward.
PRIVATE int ai_best_in_range_skill_index(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t *enemy, entity_t *target) {
    int best = -1;
    for (int i = 0; i < enemy->skill_count; i++) {
        enemy->selected_skill = i;
        if (!skill_target_in_range(allocator, grid, entities, enemy, target)) {
            continue;
        }
        if (best < 0 || ai_skill_beats(enemy->skills[i], enemy->skills[best])) {
            best = i;
        }
    }
    return best;
}

// Runs one enemy's turn:
// - find nearest ALIVE player entity via BFS rooted at the enemy's tile (skip_entity=enemy)
// - selects the enemy's preferred (highest-damage) skill and, while mp>0 and
//   out of that skill's range: BFS rooted at the TARGET's tile
//   (skip_entity=ENTITY_ID_NONE) for a distance-to-target field, step to the
//   orthogonal neighbor (order: up, right, down, left) with smallest
//   distance-to-target via action_try_move, one tile at a time, then recheck
// - once movement stops (in range, or out of mp), attacks with whichever of
//   the enemy's skills is both currently in range AND highest damage --
//   this may be a weaker skill than preferred, if preferred wasn't
//   reachable this turn
// - no-op if no alive player entities remain, or nothing ends up in range
// Returns the attacked entity, or 0 if no attack landed.
PUBLIC entity_t* ai_run_ennemy_turn(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t *enemy) {
    entity_t* target = ai_find_nearest_player(allocator, grid, entities, enemy);
    if (target == 0) {
        return 0;
    }

    enemy->selected_skill = ai_preferred_skill_index(enemy);

    while (enemy->mp > 0 && !skill_target_in_range(allocator, grid, entities, enemy, target)) {
        if (!ai_step_toward(allocator, grid, entities, enemy, target)) {
            break;
        }
    }

    int attack_skill = ai_best_in_range_skill_index(allocator, grid, entities, enemy, target);
    if (attack_skill < 0) {
        return 0;
    }
    enemy->selected_skill = attack_skill;

    return action_try_attack(allocator, grid, entities, enemy, target) ? target : 0;
}
