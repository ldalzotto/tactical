#include "game.h"

#include "../lib/assert.h"
#include "action.h"
#include "ai.h"
#include "pathing.h"
#include "render_cache.h"

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

    // Starts empty and grows on demand (see game_set_mode) -- there is no
    // fixed capacity, since a skill's range can push reachable/attack-range
    // tile counts arbitrarily high.
    slice_t scratch_region = linear_allocator_push(allocator, 0);
    linear_allocator_t scratch = linear_allocator_init(scratch_region);
    slice_t reachable_align = linear_allocator_push(&scratch, 0);
    slice_position_t reachable_tiles = LINEAR_ALLOCATOR_PUSH(&scratch, reachable_tiles, 0);
    slice_t attack_range_align = linear_allocator_push(&scratch, 0);
    slice_position_t attack_range_tiles = LINEAR_ALLOCATOR_PUSH(&scratch, attack_range_tiles, 0);

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
        .render = {
            .reachable_align = reachable_align,
            .reachable_tiles = reachable_tiles,
            .attack_range_align = attack_range_align,
            .attack_range_tiles = attack_range_tiles,
        },
    };
    return game;
}

PUBLIC void game_deinit(linear_allocator_t *allocator, game_state_t state) {
    LINEAR_ALLOCATOR_POP(&state.scratch, state.render.attack_range_tiles);
    linear_allocator_pop(&state.scratch, state.render.attack_range_align);
    LINEAR_ALLOCATOR_POP(&state.scratch, state.render.reachable_tiles);
    linear_allocator_pop(&state.scratch, state.render.reachable_align);
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

// Grows game->scratch in place, if needed, to fit `needed` more bytes at its
// current cursor (plus worst-case alignment padding, so the caller doesn't
// need to duplicate the alignment math to know it'll fit). `pathing` and
// `temp` are staged on `allocator`, directly above game->scratch's data at
// this point (nothing else is), so growing means: reserve `extra` more bytes
// on `allocator` right after game->scratch's current data, which slides
// `pathing` and `temp` up to make room -- rebase both by the same amount so
// they keep pointing at their (moved) content. Returns the byte shift
// applied, 0 if game->scratch already had enough room; callers further up
// the call stack must apply the same shift to anything else they hold above
// game->scratch (see game_on_input_event / app_dispatch_input_events).
// TODO: pass the scratch buffer instead of game structure
PRIVATE ptrdiff_t game_scratch_grow_for(linear_allocator_t *allocator, game_state_t *game, 
        pathing_state_t *pathing, 
        slice_t *temp_align, slice_position_t *temp_tiles, size_t needed) {
    size_t worst_case_padding = _Alignof(position_t) - 1;
    size_t available = (size_t)bytesize(game->scratch.cursor, game->scratch.data.end);
    size_t required = needed + worst_case_padding;
    if (required <= available) {
        return 0;
    }

    ptrdiff_t extra = (ptrdiff_t)(required - available);
    linear_allocator_insert(allocator, game->scratch.data.end, (size_t)extra);
    game->scratch.data.end = byteoffset(game->scratch.data.end, extra);

    pathing->align = slice_shift(pathing->align, extra);
    pathing->dist.slice = slice_shift(pathing->dist.slice, extra);
    *temp_align = slice_shift(*temp_align, extra);
    temp_tiles->slice = slice_shift(temp_tiles->slice, extra);

    return extra;
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
// game->scratch has no fixed capacity: both branches stage their tile list
// on `allocator` first (unbounded there), so the final size falls out of the
// scan instead of a separate dry-run pass, then grow game->scratch to fit
// (see game_scratch_grow_for) before copying the list into it. Growing can
// relocate memory above game->scratch (whatever the caller has staged there,
// e.g. app.c's in-flight input-event buffer), so the byte shift applied (0
// if none) is returned for callers to propagate and rebase against.
PRIVATE ptrdiff_t game_set_mode(game_state_t *game, linear_allocator_t *allocator, game_mode_t mode) {
    render_cache_reset(&game->scratch, &game->render);
    game->mode = mode;

    if (mode == GAME_MODE_NONE) {
        return 0;
    }

    entity_t *active = turn_active_entity(game->turn);

    if (mode == GAME_MODE_MOVEMENT) {
        if (active->mp <= 0) {
            return 0;
        }

        pathing_state_t pathing = pathing_compute_distances(allocator, game->grid, game->entities, active->position, active->mp);

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

        ptrdiff_t shift = game_scratch_grow_for(allocator, game, &pathing, &temp_align, &temp_tiles, (size_t)SLICE_BYTESIZE(temp_tiles));
        slice_t reachable_align = linear_allocator_push_alignment(&game->scratch, _Alignof(position_t));
        slice_position_t reachable_tiles = LINEAR_ALLOCATOR_PUSH(&game->scratch, game->render.reachable_tiles, SLICE_TYPESIZE(temp_tiles));
        // TODO: Ideally, the memcpy should be done inside the memory insert call inside game_scratch_grow_for
        __builtin_memcpy(reachable_tiles.begin, temp_tiles.begin, (size_t)SLICE_BYTESIZE(temp_tiles));

        render_cache_write_reachable(&game->scratch, &game->render, reachable_align, reachable_tiles);

        linear_allocator_pop(allocator, temp_tiles.slice);
        linear_allocator_pop(allocator, temp_align);
        pathing_deinit(allocator, pathing);

        return shift;
    } else {
        // mode is an enum with only NONE/MOVEMENT/ATTACK; after the two
        // returns above, ATTACK is the only remaining possibility.
        assert_debug(mode == GAME_MODE_ATTACK);

        int skill_range = SLICE_AT(active->skills, game->selected_skill).range;
        pathing_state_t pathing = pathing_compute_line_of_sight(allocator, game->grid, game->entities, active->position, skill_range);

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

        ptrdiff_t shift = game_scratch_grow_for(allocator, game, &pathing, &temp_align, &temp_tiles, (size_t)SLICE_BYTESIZE(temp_tiles));

        slice_t attack_range_align = linear_allocator_push_alignment(&game->scratch, _Alignof(position_t));
        slice_position_t attack_range_tiles = LINEAR_ALLOCATOR_PUSH(&game->scratch, game->render.attack_range_tiles, SLICE_TYPESIZE(temp_tiles));
        __builtin_memcpy(attack_range_tiles.begin, temp_tiles.begin, (size_t)SLICE_BYTESIZE(temp_tiles));

        render_cache_write_attack_range(&game->render, attack_range_align, attack_range_tiles);

        linear_allocator_pop(allocator, temp_tiles.slice);
        linear_allocator_pop(allocator, temp_align);
        pathing_deinit(allocator, pathing);

        return shift;
    }
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
                game->turn = turn_remove_dead_entities(game->turn);
            }
            game_check_game_over(game);
        }

        game->turn = turn_advance(game->turn);
        active = turn_active_entity(game->turn);
    }

    return shift;
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

    if (action_try_attack(allocator, game->grid, game->entities, active, SLICE_AT(active->skills, game->selected_skill), entity)) {
        // If the entity just died, we remove dead entities
        if (!entity->alive) {
            game->turn = turn_remove_dead_entities(game->turn);
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
    if (game->mode == GAME_MODE_NONE) {
        return 0;
    }

    // The caller (game_on_input_event) already routed occupied tiles to
    // game_on_entity_pressed; reaching here with an entity on `target` would
    // be a dispatch bug, so assert the invariant instead of silently
    // no-oping on a tile the player can't actually select.
    assert_debug(entity_find_at(game->entities, target) == 0);

    if (action_try_move(allocator, game->grid, game->entities, active, target)) {
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
// Safe to call repeatedly: game_set_mode's render_cache_reset rewinds
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
    return game_set_mode(game, allocator, game->mode);
}

PRIVATE ptrdiff_t game_on_end_turn_pressed(game_state_t *game, linear_allocator_t *allocator) {
    assert_debug(game->game_over == GAME_OVER_NONE);
    entity_t *active = turn_active_entity(game->turn);
    if (active->team != ENTITY_TEAM_PLAYER) {
        return 0;
    }

    return game_advance_turn(game, allocator);
}

// Returns the byte shift game_set_mode applied while growing game->scratch
// for this event (0 if none/no scratch-touching handler ran). Callers that
// hold anything else above game->scratch in `allocator` (app.c's in-flight
// input-event buffer) must rebase it by the same amount -- see
// app_dispatch_input_events.
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

        // Hit-test skill buttons only when render_hud would draw them (same
        // gate as there), so a click elsewhere falls through to the grid.
        entity_t *active_for_skill_buttons = turn_active_entity(game->turn);
        if (active_for_skill_buttons->team == ENTITY_TEAM_PLAYER && game->mode != GAME_MODE_NONE && entity_skill_count(active_for_skill_buttons) > 1) {
            // Clamped to VIEWPORT_MAX_SKILL_BUTTONS, same as render_hud.
            int button_count = entity_skill_count(active_for_skill_buttons);
            if (button_count > VIEWPORT_MAX_SKILL_BUTTONS) {
                button_count = VIEWPORT_MAX_SKILL_BUTTONS;
            }
            for (int i = 0; i < button_count; i++) {
                if (point_in_rect(SLICE_AT(viewport_skill_buttons(&game->viewport), i), event.x, event.y)) {
                    return game_on_skill_button_pressed(game, allocator, i);
                }
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

        return 0;
    }
}
