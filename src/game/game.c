#include "game.h"

#include "../lib/assert.h"
#include "action.h"
#include "ai.h"
#include "pathing.h"
#include "pathing_ranges.h"
#include "skill.h"

PRIVATE void game_check_game_over(game_state_t *game) {
    assert_debug(game->game_over == GAME_OVER_NONE);

    if (entity_alive_count(game->entities, ENTITY_TEAM_ENEMY) == 0) {
        game->game_over = GAME_OVER_WIN;
        return;
    }

    if (entity_alive_count(game->entities, ENTITY_TEAM_PLAYER) == 0) {
        game->game_over = GAME_OVER_LOSE;
    }
}

PUBLIC game_state_t game_init(linear_allocator_t *allocator, slice_t grid_align, grid_t grid, slice_t entity_list_align, slice_entity_t entities, slice_t skill_list_align, slice_skill_t skills, slice_t turn_order_align, slice_entity_ptr_t turn_order, int fb_width, int fb_height, int hud_height) {
    assert_debug((void*)entities.begin >= (void*)grid.tiles.end);
    assert_debug((void*)skills.begin >= (void*)entities.end);
    assert_debug((void*)turn_order.begin >= (void*)skills.end);

    viewport_t viewport = layout_compute(fb_width, fb_height, grid.width, grid.height, hud_height);

    // Starts empty and grows on demand (see game_set_mode) since a skill's
    // range can push tile counts arbitrarily high.
    slice_t scratch_region = linear_allocator_push(allocator, 0);
    linear_allocator_t scratch = linear_allocator_init(scratch_region);
    slice_t walking_distances_align = linear_allocator_push(&scratch, 0);
    pathing_state_t walking_distances = {
        .align = linear_allocator_push(&scratch, 0),
        .dist = LINEAR_ALLOCATOR_PUSH(&scratch, walking_distances.dist, 0),
    };
    slice_t reachable_align = linear_allocator_push(&scratch, 0);
    slice_position_t reachable_tiles = LINEAR_ALLOCATOR_PUSH(&scratch, reachable_tiles, 0);
    slice_t attack_range_align = linear_allocator_push(&scratch, 0);
    slice_position_t attack_range_tiles = LINEAR_ALLOCATOR_PUSH(&scratch, attack_range_tiles, 0);
    slice_t blast_preview_align = linear_allocator_push(&scratch, 0);
    slice_position_t blast_preview_tiles = LINEAR_ALLOCATOR_PUSH(&scratch, blast_preview_tiles, 0);

    game_state_t game = {
        .grid_align = grid_align,
        .grid = grid,
        .entity_list_align = entity_list_align,
        .entities = entities,
        .skill_list_align = skill_list_align,
        .skills = skills,
        .turn_order_align = turn_order_align,
        .turn = turn_init(turn_order),
        .viewport = viewport,
        .mode = GAME_MODE_NONE,
        .hover = { 0, 0 },
        .hover_valid = false,
        .selected_skill = 0,
        .game_over = GAME_OVER_NONE,
        .scratch = scratch,
        .pathing = {
            .walking_distances_align = walking_distances_align,
            .walking_distances = walking_distances,
            .reachable_align = reachable_align,
            .reachable_tiles = reachable_tiles,
            .attack_range_align = attack_range_align,
            .attack_range_tiles = attack_range_tiles,
            .blast_preview_align = blast_preview_align,
            .blast_preview_tiles = blast_preview_tiles,
        },
    };
    return game;
}

PUBLIC void game_deinit(linear_allocator_t *allocator, game_state_t state) {
    LINEAR_ALLOCATOR_POP(&state.scratch, state.pathing.blast_preview_tiles);
    linear_allocator_pop(&state.scratch, state.pathing.blast_preview_align);
    LINEAR_ALLOCATOR_POP(&state.scratch, state.pathing.attack_range_tiles);
    linear_allocator_pop(&state.scratch, state.pathing.attack_range_align);
    LINEAR_ALLOCATOR_POP(&state.scratch, state.pathing.reachable_tiles);
    linear_allocator_pop(&state.scratch, state.pathing.reachable_align);
    LINEAR_ALLOCATOR_POP(&state.scratch, state.pathing.walking_distances.dist);
    linear_allocator_pop(&state.scratch, state.pathing.walking_distances.align);
    linear_allocator_pop(&state.scratch, state.pathing.walking_distances_align);
    linear_allocator_deinit(&state.scratch);
    linear_allocator_pop(allocator, state.scratch.data);
    turn_order_deinit(allocator, state.turn.capacity);
    linear_allocator_pop(allocator, state.turn_order_align);
    skill_list_deinit(allocator, state.skills);
    linear_allocator_pop(allocator, state.skill_list_align);
    entity_list_deinit(allocator, state.entities);
    linear_allocator_pop(allocator, state.entity_list_align);
    grid_deinit(allocator, state.grid);
    linear_allocator_pop(allocator, state.grid_align);
}

// Grows `scratch` in place if needed to fit `temp_tiles`, copies
// `temp_align`/`temp_tiles` into it as `out_align`/`out_tiles`, and pops the
// caller's staged copy off `allocator`. Growing inserts bytes into
// `allocator` right above `scratch`'s data, sliding up `pathing` (if any)
// and `temp` (the only things staged there) -- rebases both. `pathing` may
// be NULL when the caller has nothing else staged above scratch on
// `allocator`. Returns the shift applied (0 if it already fit); callers
// must propagate it to anything else they hold above `scratch` (see
// game_on_input_event / app_dispatch_input_events).
PRIVATE ptrdiff_t game_scratch_push(linear_allocator_t *allocator, linear_allocator_t *scratch,
        pathing_state_t *pathing,
        slice_t temp_align, slice_position_t temp_tiles,
        slice_t *out_align, slice_position_t *out_tiles) {
    size_t needed = (size_t)SLICE_BYTESIZE(temp_tiles);
    size_t worst_case_padding = _Alignof(position_t) - 1;
    size_t available = (size_t)bytesize(scratch->cursor, scratch->data.end);
    size_t required = needed + worst_case_padding;

    ptrdiff_t shift = 0;
    if (required > available) {
        ptrdiff_t extra = (ptrdiff_t)(required - available);
        linear_allocator_insert(allocator, scratch->data.end, (size_t)extra);
        scratch->data.end = byteoffset(scratch->data.end, extra);

        if (pathing != 0) {
            pathing->align = slice_shift(pathing->align, extra);
            pathing->dist.slice = slice_shift(pathing->dist.slice, extra);
        }
        temp_align = slice_shift(temp_align, extra);
        temp_tiles.slice = slice_shift(temp_tiles.slice, extra);

        shift = extra;
    }

    // Push data to the game scratch
    *out_align = linear_allocator_push_alignment(scratch, _Alignof(position_t));
    *out_tiles = LINEAR_ALLOCATOR_PUSH(scratch, temp_tiles, SLICE_TYPESIZE(temp_tiles));
    linear_allocator_copy(scratch, temp_tiles.slice, out_tiles->slice);

    linear_allocator_pop(allocator, temp_tiles.slice);
    linear_allocator_pop(allocator, temp_align);

    return shift;
}

// Grows `scratch` in place if needed to fit `temp`'s dist array and copies
// it in as `out`. Unlike game_scratch_push, does *not* pop `temp` off
// `allocator` -- `temp` sits below a second region on `allocator`,
// `temp_align`/`temp_tiles`, that the caller isn't ready to unwind yet
// (e.g. a tile list built by scanning `temp`, staged above it and not yet
// itself persisted), so `temp` isn't the topmost thing there and can't be
// popped directly; both regions are only rebased in place when growth
// relocates memory. Once the caller has separately persisted and popped
// `temp_align`/`temp_tiles` (see game_scratch_push), `temp` becomes the
// topmost thing on `allocator` and can be popped normally. Returns the
// shift applied (0 if it already fit); callers must propagate it to
// anything else they hold above `scratch`.
PRIVATE ptrdiff_t game_scratch_stage_pathing(linear_allocator_t *allocator, linear_allocator_t *scratch,
        pathing_state_t *temp,
        slice_t *temp_align, slice_position_t *temp_tiles,
        slice_t *out_align, pathing_state_t *out) {
    size_t needed = (size_t)SLICE_BYTESIZE(temp->dist);
    size_t worst_case_padding = _Alignof(int32_t) - 1;
    size_t available = (size_t)bytesize(scratch->cursor, scratch->data.end);
    size_t required = needed + worst_case_padding;

    ptrdiff_t shift = 0;
    if (required > available) {
        ptrdiff_t extra = (ptrdiff_t)(required - available);
        linear_allocator_insert(allocator, scratch->data.end, (size_t)extra);
        scratch->data.end = byteoffset(scratch->data.end, extra);

        temp->align = slice_shift(temp->align, extra);
        temp->dist.slice = slice_shift(temp->dist.slice, extra);
        *temp_align = slice_shift(*temp_align, extra);
        temp_tiles->slice = slice_shift(temp_tiles->slice, extra);

        shift = extra;
    }

    // Push data to the game scratch. The outer alignment marker absorbs the
    // real padding, so the pathing_state_t's own internal align marker is
    // pushed zero-length right after it (see pathing_ranges.h's doc comment
    // on the two nested markers).
    *out_align = linear_allocator_push_alignment(scratch, _Alignof(int32_t));
    out->align = linear_allocator_push(scratch, 0);
    out->dist = LINEAR_ALLOCATOR_PUSH(scratch, out->dist, SLICE_TYPESIZE(temp->dist));
    linear_allocator_copy(scratch, temp->dist.slice, out->dist.slice);

    return shift;
}

// Switches to `mode`. reachable_tiles and attack_range_tiles are mutually
// exclusive, so each branch nullifies the one it isn't populating:
// - NONE: nullifies both -- nothing selected, nothing to show.
// - MOVEMENT: recomputes reachable_tiles for the turn's active entity (empty
//   if it has no mp left), nullifies attack_range_tiles. Called eagerly on
//   any selection/position/mp change, and whenever attack mode turns off.
// - ATTACK: mirror image -- nullifies reachable_tiles, computes
//   attack_range_tiles via line of sight rooted at the active entity's
//   currently-selected skill range instead of mp.
// Render just reads the cached lists, no per-frame pathing.
//
// Both branches stage their tile list on `allocator` first (unbounded),
// then grow game->scratch to fit and copy it in (see game_scratch_push).
// Growing can relocate memory above game->scratch, so the byte shift applied
// (0 if none) is returned for callers to propagate and rebase against.
PRIVATE ptrdiff_t game_set_mode(game_state_t *game, linear_allocator_t *allocator, game_mode_t mode) {
    pathing_ranges_reset(&game->scratch, &game->pathing);
    game->mode = mode;

    if (mode == GAME_MODE_NONE) {
        return 0;
    }

    entity_t *active = turn_active_entity(game->turn);

    if (mode == GAME_MODE_MOVEMENT) {
        // Computed even when active->mp <= 0 (yielding reachable_tiles
        // empty and walking_distances holding only the mover's own tile at
        // distance 0) rather than short-circuited, so
        // game->pathing.walking_distances is always a validly-sized dist
        // grid for action_try_move (F2-03) to query -- never the
        // zero-length marker pathing_ranges_reset leaves it at.
        pathing_state_t pathing = pathing_compute_walking_distances(allocator, game->grid, game->entities, active->position, active->mp);

        slice_t temp_align = linear_allocator_push_alignment(allocator, _Alignof(position_t));
        slice_position_t temp_tiles;
        temp_tiles = LINEAR_ALLOCATOR_PUSH(allocator, temp_tiles, 0);
        for (int ty = 0; ty < game->grid.height; ty++) {
            for (int tx = 0; tx < game->grid.width; tx++) {
                position_t position = { tx, ty };
                if (pathing_distance_at(pathing, game->grid, position) > 0) {
                    slice_position_t entry = LINEAR_ALLOCATOR_PUSH(allocator, temp_tiles, 1);
                    SLICE_DEREF(entry) = position;
                    temp_tiles.end = entry.end;
                }
            }
        }

        // Copy the BFS dist grid into game->scratch first (it must sit lower
        // in the stack than reachable_tiles) without yet popping it off
        // `allocator` -- temp_tiles still sits above it there. `pathing` is
        // rebased in place by this call if growth relocates it.
        slice_t walking_distances_align;
        pathing_state_t walking_distances;
        ptrdiff_t shift = game_scratch_stage_pathing(allocator, &game->scratch, &pathing, &temp_align, &temp_tiles, &walking_distances_align, &walking_distances);

        slice_t reachable_align;
        slice_position_t reachable_tiles;
        shift += game_scratch_push(allocator, &game->scratch, &pathing, temp_align, temp_tiles, &reachable_align, &reachable_tiles);
        // Reset to zero for usage sanity
        temp_align = (slice_t){0,0}; temp_tiles.slice = (slice_t){0,0};

        // temp_tiles was just popped by game_scratch_push above, so
        // `pathing`'s original allocator-resident copy is now the sole
        // remaining -- and topmost -- thing staged there; pop it now that
        // game->pathing.walking_distances holds the persisted copy.
        linear_allocator_pop(allocator, pathing.dist.slice);
        linear_allocator_pop(allocator, pathing.align);

        pathing_ranges_set_reachable(&game->scratch, &game->pathing, walking_distances_align, walking_distances, reachable_align, reachable_tiles);

        return shift;
    } else {
        // mode is an enum with only NONE/MOVEMENT/ATTACK; after the two
        // returns above, ATTACK is the only remaining possibility.
        assert_debug(mode == GAME_MODE_ATTACK);

        int skill_range = SLICE_AT(active->skills, game->selected_skill).range;

        slice_t temp_align = linear_allocator_push_alignment(allocator, _Alignof(position_t));
        slice_position_t temp_tiles;
        temp_tiles = LINEAR_ALLOCATOR_PUSH(allocator, temp_tiles, 0);
        for (int ty = 0; ty < game->grid.height; ty++) {
            for (int tx = 0; tx < game->grid.width; tx++) {
                position_t position = { tx, ty };
                // Exclude the mover's own tile -- pathing_in_range treats
                // from == to as trivially in range, but it's never a valid
                // attack target tile.
                if (position_equals(position, active->position)) {
                    continue;
                }
                if (pathing_in_range(game->grid, game->entities, active->position, position, skill_range)) {
                    slice_position_t entry = LINEAR_ALLOCATOR_PUSH(allocator, temp_tiles, 1);
                    SLICE_DEREF(entry) = position;
                    temp_tiles.end = entry.end;
                }
            }
        }

        slice_t attack_range_align;
        slice_position_t attack_range_tiles;
        ptrdiff_t shift = game_scratch_push(allocator, &game->scratch, 0, temp_align, temp_tiles, &attack_range_align, &attack_range_tiles);
        // Reset to zero for usage sanity
        temp_align = (slice_t){0,0}; temp_tiles.slice = (slice_t){0,0};

        pathing_ranges_set_attack_range(&game->scratch, &game->pathing, attack_range_align, attack_range_tiles);

        return shift;
    }
}

// Computes the blast footprint centered on `impact` and stages it into
// `scratch` as the new blast_preview_tiles (see game_scratch_push and
// pathing_ranges_set_blast_preview). Takes only the primitives it needs
// rather than game_state_t -- callers decide eligibility (mode, hover
// validity, selected skill) themselves before calling this.
PRIVATE ptrdiff_t game_stage_blast_preview(linear_allocator_t *allocator, linear_allocator_t *scratch, pathing_ranges_t *pathing, grid_t grid, position_t impact, int aoe_radius) {
    slice_t temp_align = linear_allocator_push_alignment(allocator, _Alignof(position_t));
    slice_position_t temp_tiles = pathing_compute_blast_tiles(allocator, grid, impact, aoe_radius);

    slice_t blast_preview_align;
    slice_position_t blast_preview_tiles;
    ptrdiff_t shift = game_scratch_push(allocator, scratch, 0, temp_align, temp_tiles, &blast_preview_align, &blast_preview_tiles);
    // Reset to zero for usage sanity
    temp_align = (slice_t){0,0}; temp_tiles.slice = (slice_t){0,0};

    pathing_ranges_set_blast_preview(scratch, pathing, blast_preview_align, blast_preview_tiles, impact);

    return shift;
}

// Advances the cursor past the entity that just finished acting, then lets
// the AI play out every consecutive enemy turn until either a player entity
// becomes active or the game ends.
PRIVATE ptrdiff_t game_advance_turn(game_state_t *game, linear_allocator_t *allocator) {
    game->turn = turn_advance(game->turn);
    game->selected_skill = 0;
    ptrdiff_t shift = game_set_mode(game, allocator, GAME_MODE_NONE);

    entity_t *active = turn_active_entity(game->turn);
    while (game->game_over == GAME_OVER_NONE && active->team == ENTITY_TEAM_ENEMY) {
        entity_t *attacked = ai_run_ennemy_turn(allocator, game->grid, game->entities, active);
        if (attacked != 0) {
            // If the entity just died, we remove dead entities
            if (!attacked->alive) {
                slice_t dead_align = linear_allocator_push_alignment(allocator, _Alignof(entity_ptr_t));
                slice_entity_ptr_t dead;
                dead = LINEAR_ALLOCATOR_PUSH(allocator, dead, 1);
                SLICE_DEREF(dead) = attacked;

                game->turn = turn_remove_dead_entities(game->turn, dead);

                linear_allocator_pop(allocator, dead.slice);
                linear_allocator_pop(allocator, dead_align);
            }
            game_check_game_over(game);
        }

        game->turn = turn_advance(game->turn);
        active = turn_active_entity(game->turn);
    }

    return shift;
}

// Casts an AoE skill centered on `impact` via action_try_attack_area, then
// reconciles turn order for every casualty (removing each dead entity from
// game->turn) and checks game over exactly once (not per-casualty --
// game_check_game_over asserts game->game_over == GAME_OVER_NONE on entry).
// active->team can never appear in out_hit (action_try_attack_area excludes
// same-team damage), so the active entity is never among the casualties
// reconciled here. Only called once the AoE precondition (player-controlled
// active entity, GAME_MODE_ATTACK, skill.aoe_radius > 0) already holds.
PRIVATE ptrdiff_t game_cast_attack_area(game_state_t *game, linear_allocator_t *allocator, entity_t *active, skill_t skill, position_t impact) {
    // Reuse the staged blast preview (F2-04) when it was computed for this
    // exact impact tile -- a hover event isn't guaranteed to have preceded
    // this click at all, let alone at the same tile, so the tag is checked
    // rather than trusted. Otherwise compute it fresh here, staged on
    // `allocator` below hit_align (pushed next) so action_try_attack_area's
    // hit list always lands directly at hit_align's aligned cursor, the top
    // of the allocator, regardless of which path was taken.
    bool cached_blast_valid = game->pathing.blast_preview_valid
        && position_equals(game->pathing.blast_preview_impact, impact);
    slice_t blast_align = { 0 };
    slice_position_t blast_tiles = game->pathing.blast_preview_tiles;
    if (!cached_blast_valid) {
        blast_align = linear_allocator_push_alignment(allocator, _Alignof(position_t));
        blast_tiles = pathing_compute_blast_tiles(allocator, game->grid, impact, skill.aoe_radius);
    }

    // action_try_attack_area requires allocator's cursor pre-aligned to
    // _Alignof(entity_ptr_t) (it does not self-align -- see its doc
    // comment); paired with the pops below so this fully unwinds either way.
    slice_t hit_align = linear_allocator_push_alignment(allocator, _Alignof(entity_ptr_t));

    slice_entity_ptr_t out_hit;
    if (!action_try_attack_area(allocator, game->entities, active, skill, impact, game->pathing.attack_range_tiles, blast_tiles, &out_hit)) {
        linear_allocator_pop(allocator, hit_align);
        if (!cached_blast_valid) {
            linear_allocator_pop(allocator, blast_tiles.slice);
            linear_allocator_pop(allocator, blast_align);
        }
        return 0;
    }

    // dead sits directly above out_hit, both entity_ptr_t lists, so it's
    // already aligned -- no extra alignment push needed here.
    slice_entity_ptr_t dead;
    dead = LINEAR_ALLOCATOR_PUSH(allocator, dead, 0);
    for (SLICE_FOREACH(out_hit, hit_s)) {
        entity_t *hit = SLICE_DEREF(hit_s);
        if (!hit->alive) {
            slice_entity_ptr_t entry = LINEAR_ALLOCATOR_PUSH_GROW(allocator, &dead, 1);
            SLICE_DEREF(entry) = hit;
        }
    }
    game->turn = turn_remove_dead_entities(game->turn, dead);
    game_check_game_over(game);

    linear_allocator_pop(allocator, dead.slice);
    linear_allocator_pop(allocator, out_hit.slice);
    linear_allocator_pop(allocator, hit_align);
    if (!cached_blast_valid) {
        linear_allocator_pop(allocator, blast_tiles.slice);
        linear_allocator_pop(allocator, blast_align);
    }

    return game_set_mode(game, allocator, GAME_MODE_MOVEMENT);
}

PRIVATE ptrdiff_t game_on_entity_pressed(game_state_t *game, linear_allocator_t *allocator, entity_t* entity) {
    assert_debug(game->game_over == GAME_OVER_NONE);
    assert_debug(entity != 0);
    assert_debug(entity->alive);

    entity_t *active = turn_active_entity(game->turn);
    if (active->team != ENTITY_TEAM_PLAYER) {
        return 0;
    }

    entity_t *pressed = entity;
    if (pressed == active) {
        return game_set_mode(game, allocator, GAME_MODE_MOVEMENT);
    }

    if (game->mode != GAME_MODE_ATTACK) {
        return 0;
    }

    skill_t skill = SLICE_AT(active->skills, game->selected_skill);
    if (skill_is_aoe(skill)) {
        return game_cast_attack_area(game, allocator, active, skill, entity->position);
    }

    if (action_try_attack(active, skill, entity, game->pathing.attack_range_tiles)) {
        // If the entity just died, we remove dead entities
        if (!entity->alive) {
            slice_t dead_align = linear_allocator_push_alignment(allocator, _Alignof(entity_ptr_t));
            slice_entity_ptr_t dead;
            dead = LINEAR_ALLOCATOR_PUSH(allocator, dead, 1);
            SLICE_DEREF(dead) = entity;

            game->turn = turn_remove_dead_entities(game->turn, dead);

            linear_allocator_pop(allocator, dead.slice);
            linear_allocator_pop(allocator, dead_align);
        }
        game_check_game_over(game);
        return game_set_mode(game, allocator, GAME_MODE_MOVEMENT);
    }

    return 0;
}

PRIVATE ptrdiff_t game_on_tile_pressed(game_state_t *game, linear_allocator_t *allocator, position_t target) {
    assert_debug(game->game_over == GAME_OVER_NONE);
    entity_t *active = turn_active_entity(game->turn);
    if (active->team != ENTITY_TEAM_PLAYER) {
        return 0;
    }

    if (game->mode == GAME_MODE_ATTACK) {
        // AoE skills are cast by clicking any in-range tile (empty or
        // occupied); non-AoE skills keep the entity-only-click behavior
        // above and just no-op on a tile press in attack mode.
        skill_t skill = SLICE_AT(active->skills, game->selected_skill);
        if (skill_is_aoe(skill)) {
            return game_cast_attack_area(game, allocator, active, skill, target);
        }
        return 0;
    }

    if (game->mode != GAME_MODE_MOVEMENT) {
        return 0;
    }

    // The caller (game_on_input_event) already routed occupied tiles to
    // game_on_entity_pressed; reaching here with an entity on `target` would
    // be a dispatch bug in the movement path (the AoE path above legally
    // allows an occupied impact tile), so assert the invariant instead of
    // silently no-oping on a tile the player can't actually select.
    assert_debug(entity_find_at(game->entities, target) == 0);

    if (action_try_move(game->pathing.walking_distances, game->grid, active, target)) {
        return game_set_mode(game, allocator, GAME_MODE_MOVEMENT);
    }

    return 0;
}

PRIVATE ptrdiff_t game_on_attack_toggle_pressed(game_state_t *game, linear_allocator_t *allocator) {
    assert_debug(game->game_over == GAME_OVER_NONE);
    entity_t *active = turn_active_entity(game->turn);
    if (active->team != ENTITY_TEAM_PLAYER) {
        return 0;
    }
    if (game->mode == GAME_MODE_NONE) {
        return 0;
    }

    return game_set_mode(game, allocator, game->mode == GAME_MODE_ATTACK ? GAME_MODE_MOVEMENT : GAME_MODE_ATTACK);
}

// Sets game->selected_skill to index and recomputes the range preview.
// No-op if not player-controlled, no mode active, or index out of range.
// Safe to call repeatedly: game_set_mode's pathing_ranges_reset rewinds
// game->scratch before each re-push.
PRIVATE ptrdiff_t game_on_skill_button_pressed(game_state_t *game, linear_allocator_t *allocator, int index) {
    assert_debug(game->game_over == GAME_OVER_NONE);
    entity_t *active = turn_active_entity(game->turn);
    // game_on_input_event only calls this once the same conditions hold (see
    // the hit-test gate there), so these are preconditions, not runtime
    // no-ops: assert them to catch dispatch bugs without leaving dead
    // coverage-only branches in the call path.
    assert_debug(active->team == ENTITY_TEAM_PLAYER);
    assert_debug(game->mode != GAME_MODE_NONE);
    assert_debug(index >= 0);
    assert_debug(index < entity_skill_count(active));

    game->selected_skill = index;
    ptrdiff_t shift = game_set_mode(game, allocator, game->mode);

    // Refresh the preview at the current hover tile for the newly selected
    // skill, without requiring the mouse to move -- game_set_mode itself
    // only clears blast_preview_tiles, it doesn't recompute it.
    skill_t skill = SLICE_AT(active->skills, index);
    pathing_ranges_clear_blast_preview(&game->scratch, &game->pathing);
    if (game->mode == GAME_MODE_ATTACK && game->hover_valid && skill_is_aoe(skill)
            && skill_can_target_area(game->grid, game->entities, active, skill, game->hover)) {
        shift += game_stage_blast_preview(allocator, &game->scratch, &game->pathing, game->grid, game->hover, skill.aoe_radius);
    }
    return shift;
}

PRIVATE ptrdiff_t game_on_end_turn_pressed(game_state_t *game, linear_allocator_t *allocator) {
    assert_debug(game->game_over == GAME_OVER_NONE);
    entity_t *active = turn_active_entity(game->turn);
    if (active->team != ENTITY_TEAM_PLAYER) {
        return 0;
    }

    return game_advance_turn(game, allocator);
}

// Returns the byte shift applied while growing game->scratch for this event
// (0 if none). Callers holding anything else above game->scratch in
// `allocator` must rebase it -- see app_dispatch_input_events.
PUBLIC ptrdiff_t game_on_input_event(game_state_t *game, linear_allocator_t *allocator, input_event_t event) {
    if (game->game_over != GAME_OVER_NONE) {
        return 0;
    }

    if (event.type == INPUT_EVENT_MOUSE_CLICK) {
        if (point_in_rect(game->viewport.end_turn_button, event.x, event.y)) {
            return game_on_end_turn_pressed(game, allocator);
        }

        if (point_in_rect(game->viewport.attack_button, event.x, event.y)) {
            return game_on_attack_toggle_pressed(game, allocator);
        }

        // layout_visible_skill_button_count keeps this in sync with render_hud.
        entity_t *active_for_skill_buttons = turn_active_entity(game->turn);
        int button_count = layout_visible_skill_button_count(
            active_for_skill_buttons->team == ENTITY_TEAM_PLAYER, game->mode != GAME_MODE_NONE, entity_skill_count(active_for_skill_buttons));
        for (int i = 0; i < button_count; i++) {
            if (point_in_rect(SLICE_AT(viewport_skill_buttons(&game->viewport), i), event.x, event.y)) {
                return game_on_skill_button_pressed(game, allocator, i);
            }
        }

        int tx, ty;
        if (!screen_to_grid(game->viewport, event.x, event.y, &tx, &ty)) {
            return 0;
        }

        position_t target = { tx, ty };
        entity_t* found = entity_find_at(game->entities, target);
        if (found != 0) {
            return game_on_entity_pressed(game, allocator, found);
        } else {
            return game_on_tile_pressed(game, allocator, target);
        }
    } else {
        // input_event_type_t only has MOUSE_MOVE and MOUSE_CLICK, so after
        // the click branch above this is always a mouse move.
        assert_debug(event.type == INPUT_EVENT_MOUSE_MOVE);

        int tx, ty;
        bool valid = screen_to_grid(game->viewport, event.x, event.y, &tx, &ty);
        game->hover_valid = valid;
        if (valid) {
            game->hover = (position_t){ tx, ty };
        }

        // mode can only be GAME_MODE_ATTACK when the active entity is
        // player-controlled: game_on_attack_toggle_pressed and
        // game_on_skill_button_pressed (game_set_mode's only two callers that
        // can leave mode at ATTACK) both gate on active->team ==
        // ENTITY_TEAM_PLAYER before ever calling it with ATTACK. Asserted
        // rather than re-checked here since MOUSE_MOVE (unlike those two)
        // fires regardless of whose turn it is.
        pathing_ranges_clear_blast_preview(&game->scratch, &game->pathing);
        if (game->mode == GAME_MODE_ATTACK) {
            entity_t *active = turn_active_entity(game->turn);
            skill_t skill = SLICE_AT(active->skills, game->selected_skill);
            assert_debug(active->team == ENTITY_TEAM_PLAYER);
            if (game->hover_valid && skill_is_aoe(skill)
                    && skill_can_target_area(game->grid, game->entities, active, skill, game->hover)) {
                return game_stage_blast_preview(allocator, &game->scratch, &game->pathing, game->grid, game->hover, skill.aoe_radius);
            }
        }
        return 0;
    }
}
