#include "ai.h"

#include "action.h"

/*
    Iterate over the adjacent tiles of (x,y).
    Return the nearest tile.
*/
static int ai_distance_to_adjacency(pathing_state_t pathing, grid_t grid, int x, int y) {
    static const int dx[4] = { 0, 1, 0, -1 };  // up, right, down, left
    static const int dy[4] = { -1, 0, 1, 0 };

    int best = -1;
    for (int dir = 0; dir < 4; dir++) {
        int nx = x + dx[dir];
        int ny = y + dy[dir];
        if (!grid_in_bounds(grid, nx, ny)) {
            continue;
        }
        int d = pathing_distance_at(pathing, grid, nx, ny);
        if (d < 0) {
            continue;
        }
        if (best < 0 || d < best) {
            best = d;
        }
    }

    return best;
}

static entity_t* ai_find_nearest_player(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t* enemy) {
    int max_steps = grid.width * grid.height;
    pathing_state_t pathing = pathing_compute_distances(allocator, grid, entities, enemy, enemy->x, enemy->y, max_steps);

    entity_t* best_entity = 0;
    int best_dist = -1;

    for ( SLICE_FOREACH(entities, candidate_s) ) {
        entity_t *candidate = &SLICE_DEREF(candidate_s);
        if (!candidate->alive || candidate->team != ENTITY_TEAM_PLAYER) {
            continue;
        }

        int dist = ai_distance_to_adjacency(pathing, grid, candidate->x, candidate->y);
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

static bool ai_step_toward(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t* enemy, entity_t *target) {
    int max_steps = grid.width * grid.height;
    // We compute the distance from the target.
    // The smallest distance of ennemy neighbor is the tile we are going to move towards.
    pathing_state_t pathing = pathing_compute_distances(allocator, grid, entities, target, target->x, target->y, max_steps);

    // TODO: this pattern appears at a lot of places
    static const int dx[4] = { 0, 1, 0, -1 };  // up, right, down, left
    static const int dy[4] = { -1, 0, 1, 0 };

    bool found = false;
    int best_dist = -1;
    int best_x = 0, best_y = 0;

    for (int dir = 0; dir < 4; dir++) {
        int nx = enemy->x + dx[dir];
        int ny = enemy->y + dy[dir];
        if (!grid_in_bounds(grid, nx, ny)) {
            continue;
        }

        int dist = pathing_distance_at(pathing, grid, nx, ny);
        if (dist < 0) {
            continue;
        }

        if (!found || dist < best_dist) {
            found = true;
            best_dist = dist;
            best_x = nx;
            best_y = ny;
        }
    }

    pathing_deinit(allocator, pathing);

    if (!found) {
        return false;
    }

    return action_try_move(allocator, grid, entities, enemy, best_x, best_y);
}

// For every ALIVE enemy (ascending entity_id): find nearest ALIVE player
// entity via BFS rooted at the enemy's own tile (skip_entity=enemy). If
// already orthogonally adjacent and ap>0, action_try_attack. Otherwise, while
// mp>0 and not yet adjacent: BFS rooted at the TARGET's tile
// (skip_entity=ENTITY_ID_NONE) to get a distance-to-target field, step to
// whichever orthogonal neighbor (checked in order: up, right, down, left) of
// the enemy's current tile has the smallest distance-to-target via
// action_try_move (one tile at a time), then re-check adjacency/attack. If no
// alive player entities remain, no-op for all enemies.
void ai_run_enemy_phase(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities) {
    for ( SLICE_FOREACH(entities, ennemy_s) ) {
        entity_t *enemy = &SLICE_DEREF(ennemy_s);

        if (!enemy->alive || enemy->team != ENTITY_TEAM_ENEMY) {
            continue;
        }

        entity_t* target = ai_find_nearest_player(allocator, grid, entities, enemy);
        if (target == 0) {
            continue;
        }

        if (entity_is_adjacent(*enemy, *target)) {
            action_try_attack(enemy, target);
            continue;
        }

        while (enemy->mp > 0 && !entity_is_adjacent(*enemy, *target)) {
            if (!ai_step_toward(allocator, grid, entities, enemy, target)) {
                break;
            }
        }

        if (entity_is_adjacent(*enemy, *target)) {
            action_try_attack(enemy, target);
        }
    }
}
