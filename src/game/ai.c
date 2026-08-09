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

static entity_id_t ai_find_nearest_player(grid_t grid, entity_list_t entities, pathing_state_t pathing, entity_id_t enemy_id) {
    entity_t *enemy = entity_at(entities, enemy_id);

    int max_steps = grid.width * grid.height;
    pathing_compute_distances(pathing, grid, entities, enemy_id, enemy->x, enemy->y, max_steps);

    entity_id_t best_id = ENTITY_ID_NONE;
    int best_dist = -1;

    for (int i = 0; i < entities.count; i++) {
        entity_t *candidate = entity_at(entities, (entity_id_t)i);
        if (!candidate->alive || candidate->team != ENTITY_TEAM_PLAYER) {
            continue;
        }

        int dist = ai_distance_to_adjacency(pathing, grid, candidate->x, candidate->y);
        if (dist < 0) {
            continue;
        }

        if (best_id == ENTITY_ID_NONE || dist < best_dist) {
            best_id = (entity_id_t)i;
            best_dist = dist;
        }
    }

    return best_id;
}

static bool ai_step_toward(grid_t grid, entity_list_t entities, pathing_state_t pathing, entity_id_t enemy_id, entity_t *target) {
    int max_steps = grid.width * grid.height;
    pathing_compute_distances(pathing, grid, entities, ENTITY_ID_NONE, target->x, target->y, max_steps);

    entity_t *enemy = entity_at(entities, enemy_id);

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

    if (!found) {
        return false;
    }

    return action_try_move(grid, entities, pathing, enemy_id, best_x, best_y);
}

void ai_run_enemy_phase(grid_t grid, entity_list_t entities, pathing_state_t pathing) {
    for (int i = 0; i < entities.count; i++) {
        entity_id_t enemy_id = (entity_id_t)i;
        entity_t *enemy = entity_at(entities, enemy_id);

        if (!enemy->alive || enemy->team != ENTITY_TEAM_ENEMY) {
            continue;
        }

        entity_id_t target_id = ai_find_nearest_player(grid, entities, pathing, enemy_id);
        if (target_id == ENTITY_ID_NONE) {
            continue;
        }

        entity_t *target = entity_at(entities, target_id);

        if (entity_is_adjacent(*enemy, *target)) {
            action_try_attack(entities, enemy_id, target_id);
            continue;
        }

        while (enemy->mp > 0 && !entity_is_adjacent(*enemy, *target)) {
            if (!ai_step_toward(grid, entities, pathing, enemy_id, target)) {
                break;
            }
        }

        if (entity_is_adjacent(*enemy, *target)) {
            action_try_attack(entities, enemy_id, target_id);
        }
    }
}
