#include "ai.h"

#include "action.h"

// Distance from an entity to a candidate tile, via a BFS field rooted at the
// entity itself: occupied tiles (including the candidate's own tile, since
// it's occupied by the candidate) block passage except at the BFS root, so
// the candidate's own tile is never reachable in that field. Instead take
// the minimum distance among the candidate's four orthogonal neighbor
// tiles -- this is 0 exactly when the candidate is already adjacent to the
// BFS root (one of those neighbors *is* the root tile).
static int ai_distance_to_adjacency(pathing_state_t pathing, grid_t grid, int x, int y) {
    static const int dx[4] = { 0, 1, 0, -1 };  // up, right, down, left
    static const int dy[4] = { -1, 0, 1, 0 };

    int best = -1;
    for (int dir = 0; dir < 4; dir++) {
        int d = pathing_distance_at(pathing, grid, x + dx[dir], y + dy[dir]);
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
    pathing_state_t pathing = pathing_compute_distances(allocator, grid, entities, 0, target->x, target->y, max_steps);

    static const int dx[4] = { 0, 1, 0, -1 };  // up, right, down, left
    static const int dy[4] = { -1, 0, 1, 0 };

    bool found = false;
    int best_dist = -1;
    int best_x = 0, best_y = 0;

    for (int dir = 0; dir < 4; dir++) {
        int nx = enemy->x + dx[dir];
        int ny = enemy->y + dy[dir];

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
