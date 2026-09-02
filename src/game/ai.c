#include "ai.h"

#include "action.h"
#include "game/entity.h"
#include "game/grid.h"
#include "game/position.h"
#include "game/turn.h"
#include "lib/linkage.h"
#include "lib/memory.h"
#include "pathing.h"
#include "skill.h"

#include "../lib/assert.h"
#include <stddef.h>

// Below this HP%, and lacking a lethal shot at its chosen target, an enemy
// retreats instead of closing/attacking.
#define AI_RETREAT_HP_PERCENT 25

// Safety bound on ai_run_ennemy_turn's action loop -- each iteration must
// spend ap or mp to continue (see the progress check), so this only exists
// to guarantee termination if that invariant is ever violated.
#define AI_MAX_ACTIONS_PER_TURN 8

// Target-scoring weights (see ai_score_target). Plain constants for now --
// no designer-facing config surface yet.
#define AI_SCORE_LETHAL 1000
#define AI_SCORE_THREAT_PER_DAMAGE 5
#define AI_SCORE_DISTANCE_PER_STEP 10

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

// `target`'s own highest skill damage -- how dangerous it is to stand next
// to, used both as a target-scoring factor (threat) and a retreat trigger.
PRIVATE int ai_entity_threat(entity_t *target) {
    int threat = 0;
    for (SLICE_FOREACH(target->skills, skill_s)) {
        skill_t *skill = &SLICE_DEREF(skill_s);
        if (skill->damage > threat) {
            threat = skill->damage;
        }
    }
    return threat;
}

// Weighted desirability of attacking `candidate` this turn, given `dist`
// (BFS steps to adjacency, see ai_distance_to_adjacency): a big flat bonus
// for a lethal hit with the enemy's preferred skill, then missing-HP% (favor
// weakened targets), then the candidate's own threat (favor dangerous
// targets), minus a per-step distance cost (favor reachable-now targets).
// Deliberately plain int arithmetic -- see AI_SCORE_* above.
PRIVATE int ai_score_target(skill_t *preferred, entity_t *candidate, int dist) {
    int hp_missing_percent = (candidate->max_hp - candidate->hp) * 100 / candidate->max_hp;
    int threat = ai_entity_threat(candidate);

    int score = hp_missing_percent + threat * AI_SCORE_THREAT_PER_DAMAGE - dist * AI_SCORE_DISTANCE_PER_STEP;
    if (preferred->damage >= candidate->hp) {
        score += AI_SCORE_LETHAL;
    }
    return score;
}

// Best-scoring reachable player (see ai_score_target), or 0 if none are
// reachable. Replaces plain nearest-player selection with target
// prioritization: a weakened, dangerous, or killable-now target can win out
// over a merely-closer one.
PRIVATE entity_t* ai_choose_best_target(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t* enemy) {
    int max_steps = grid.width * grid.height;
    pathing_state_t pathing = pathing_compute_walking_distances(allocator, grid, entities, enemy->position, max_steps);

    skill_t *preferred = ai_preferred_skill(enemy);

    entity_t* best_entity = 0;
    int best_score = 0;

    for ( SLICE_FOREACH(entities, candidate_s) ) {
        entity_t *candidate = &SLICE_DEREF(candidate_s);
        if (!candidate->alive || candidate->team != ENTITY_TEAM_PLAYER) {
            continue;
        }

        int dist = ai_distance_to_adjacency(pathing, grid, candidate->position);
        if (dist < 0) {
            continue;
        }

        int score = ai_score_target(preferred, candidate, dist);
        if (best_entity == 0 || score > best_score) {
            best_entity = candidate;
            best_score = score;
        }
    }

    pathing_deinit(allocator, pathing);

    return best_entity;
}

// True if, this action, the enemy should back off from `target` instead of
// closing/attacking: its own hp is below AI_RETREAT_HP_PERCENT, it doesn't
// already have a lethal shot on `target` lined up, and `target` threatens
// enough damage (see ai_entity_threat) to plausibly kill it back.
PRIVATE bool ai_should_retreat(grid_t grid, slice_entity_t entities, entity_t *enemy, entity_t *target) {
    if (enemy->hp * 100 / enemy->max_hp > AI_RETREAT_HP_PERCENT) {
        return false;
    }

    skill_t *in_range = ai_best_in_range_skill(grid, entities, enemy, target);
    if (in_range != 0 && in_range->damage >= target->hp) {
        return false; // already lined up for the kill -- take it instead of retreating
    }

    return ai_entity_threat(target) >= enemy->hp;
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

    // ai_run_ennemy_turn only calls us after ai_choose_best_target found a
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

// Retreat counterpart of ai_step_toward: moves to whichever of the enemy's
// own reachable neighbor tiles is *farthest* (by straight-line Manhattan
// distance) from `target`, instead of closest.
//
// Unlike ai_step_toward, this can't BFS from `target` to rank the enemy's
// neighbors: in a corridor no wider than the enemy itself, that BFS has to
// pass through the enemy's own tile to reach the far side, and any alive
// entity blocks passage through its tile (see pathing_compute_walking_distances)
// -- the enemy would block its own escape route. Ranking by plain Manhattan
// distance instead sidesteps that; reachability is still checked against a
// BFS rooted at the enemy itself, which self-exempts.
PRIVATE void ai_step_away(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t* enemy, entity_t *target) {
    int max_steps = grid.width * grid.height;
    pathing_state_t pathing = pathing_compute_walking_distances(allocator, grid, entities, enemy->position, max_steps);

    bool found = false;
    int best_manhattan = -1;
    position_t best_position = { 0, 0 };

    for (SLICE_FOREACH(POSITION_DIRECTIONS, dir_s)) {
        position_t dir = SLICE_DEREF(dir_s);
        position_t neighbor = position_add(enemy->position, dir);
        if (!grid_in_bounds(grid, neighbor)) {
            continue;
        }
        if (pathing_distance_at(pathing, grid, neighbor) < 0) {
            continue; // not walkable, or occupied by another entity
        }

        int dx = neighbor.x - target->position.x;
        int dy = neighbor.y - target->position.y;
        int manhattan = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);

        if (!found || manhattan > best_manhattan) {
            found = true;
            best_manhattan = manhattan;
            best_position = neighbor;
        }
    }

    pathing_deinit(allocator, pathing);

    if (!found) {
        return; // boxed in -- no reachable neighbor to retreat to
    }

    pathing_state_t move_distances = pathing_compute_walking_distances(allocator, grid, entities, enemy->position, enemy->mp);
    action_try_move(move_distances, grid, enemy, best_position);
    pathing_deinit(allocator, move_distances);
}

// AoE counterpart of the plain action_try_attack call in ai_run_ennemy_turn:
// casts `skill` centered on `impact`, growing the caller-owned `dead`
// accumulator with any resulting casualties (see ai_run_ennemy_turn --
// a turn's actions all land in the same accumulator, not a fresh one each
// call, so multi-action turns report every casualty in one returned slice).
//
// `dead` sits under temp allocations on the stack, so it's grown via
// linear_allocator_insert (shifts the temp regions up) instead of push-and-grow.
PRIVATE void ai_try_attack_area(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t *enemy, skill_t skill, position_t impact, slice_entity_ptr_t *dead) {
    slice_t blast_align = linear_allocator_push_alignment(allocator, _Alignof(position_t));
    slice_position_t blast_tiles = pathing_compute_blast_tiles(allocator, grid, impact, skill.aoe_radius);

    // `impact` was already range/LOS-validated by ai_best_in_range_skill,
    // so a single-tile range set satisfies the in-range check.
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
            slice_entity_ptr_t inserted = LINEAR_ALLOCATOR_INSERT(allocator, *dead, dead_count);
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
            dead->end = write.begin;
        }

        linear_allocator_pop(allocator, out_hit.slice);
    }
    linear_allocator_pop(allocator, hit_align);
    linear_allocator_pop(allocator, blast_tiles.slice);
    linear_allocator_pop(allocator, blast_align);
}

// One action within an enemy's turn: pick the best-scoring reachable
// target, then either retreat from it (ai_should_retreat) or close in
// (ai_step_toward) and attack with whatever's in range once close enough.
// Grows `dead` with any casualties. Returns false if nothing changed (no
// target, no attack landed, no movement happened) -- the turn-level loop
// uses this to know when to stop.
PRIVATE bool ai_run_ennemy_action(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t *enemy, slice_entity_ptr_t *dead) {
    entity_t* target = ai_choose_best_target(allocator, grid, entities, enemy);
    if (target == 0) {
        return false;
    }

    int ap_before = enemy->ap;
    int mp_before = enemy->mp;

    if (ai_should_retreat(grid, entities, enemy, target)) {
        if (enemy->mp > 0) {
            ai_step_away(allocator, grid, entities, enemy, target);
        }
        return enemy->mp != mp_before;
    }

    skill_t *preferred = ai_preferred_skill(enemy);
    while (enemy->mp > 0 && !skill_can_target(grid, entities, enemy, *preferred, target)) {
        ai_step_toward(allocator, grid, entities, enemy, target);
    }

    skill_t *attack_skill = ai_best_in_range_skill(grid, entities, enemy, target);
    if (attack_skill == 0) {
        return enemy->mp != mp_before;
    }

    if (skill_is_aoe(*attack_skill)) {
        ai_try_attack_area(allocator, grid, entities, enemy, *attack_skill, target->position, dead);
    } else {
        // Single-tile range set for the position already confirmed in range.
        position_t attack_range_tile[1] = { target->position };
        slice_position_t attack_range_tiles = {
            .begin = attack_range_tile,
            .end = typeoffset(attack_range_tile, 1),
        };

        // dead is still the allocator's top here, so growing it in place is safe.
        if (action_try_attack(enemy, *attack_skill, target, attack_range_tiles) && !target->alive) {
            slice_entity_ptr_t entry = LINEAR_ALLOCATOR_PUSH_GROW(allocator, dead, 1);
            SLICE_DEREF(entry) = target;
        }
    }

    return enemy->ap != ap_before || enemy->mp != mp_before;
}

// Runs one enemy's turn as a bounded sequence of actions (see
// ai_run_ennemy_action): re-scores its target and acts again as long as ap
// or mp remain and the last action made progress, so an enemy with AP/MP to
// spare keeps fighting (or keeps retreating) instead of stopping after one
// move+attack. Returns every entity killed across the whole turn. See ai.h.
PUBLIC slice_entity_ptr_t ai_run_ennemy_turn(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t *enemy) {
    slice_entity_ptr_t dead = LINEAR_ALLOCATOR_PUSH(allocator, dead, 0);

    for (int actions = 0; actions < AI_MAX_ACTIONS_PER_TURN; actions++) {
        if (enemy->ap <= 0 && enemy->mp <= 0) {
            break;
        }
        if (!ai_run_ennemy_action(allocator, grid, entities, enemy, &dead)) {
            break;
        }
    }

    return dead;
}
