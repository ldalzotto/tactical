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
    pathing_ranges_t pathing = pathing_ranges_init(&scratch);

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
        .pathing = pathing,
    };
    return game;
}

PUBLIC void game_deinit(linear_allocator_t *allocator, game_state_t state) {
    pathing_ranges_deinit(&state.scratch, state.pathing);
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

// Switches to `mode`. walking_distances and attack_range_tiles are mutually
// exclusive, so each branch nullifies the one it isn't populating: NONE
// nullifies both; MOVEMENT recomputes walking_distances for the turn's
// active entity and nullifies attack_range_tiles; ATTACK is the mirror
// image, computing attack_range_tiles via line of sight over the selected
// skill's range. Render just reads the cached data, no per-frame pathing.
//
// Both branches stage their tile data on `allocator` first, then grow
// game->scratch to fit and copy it in (see
// pathing_ranges_push_walking_distances / pathing_ranges_push_attack_range).
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
        // Always computed, even when active->mp <= 0, so
        // game->pathing.walking_distances is always a valid dist grid for
        // action_try_move and render.c to query, never the empty marker
        // pathing_ranges_reset leaves it at.
        pathing_state_t pathing = pathing_compute_walking_distances(allocator, game->grid, game->entities, active->position, active->mp);

        return pathing_ranges_push_walking_distances(allocator, &game->scratch, &game->pathing, &pathing);
    } else {
        // mode is an enum with only NONE/MOVEMENT/ATTACK; after the two
        // returns above, ATTACK is the only remaining possibility.
        assert_debug(mode == GAME_MODE_ATTACK);

        skill_t skill = SLICE_AT(active->skills, game->selected_skill);

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
                if (pathing_in_range(game->grid, game->entities, active->position, position, skill.range)) {
                    slice_position_t entry = LINEAR_ALLOCATOR_PUSH(allocator, temp_tiles, 1);
                    SLICE_DEREF(entry) = position;
                    temp_tiles.end = entry.end;
                }
            }
        }

        ptrdiff_t shift = pathing_ranges_push_attack_range(allocator, &game->scratch, &game->pathing, temp_align, temp_tiles);
        // Reset to zero for usage sanity
        temp_align = (slice_t){0,0}; temp_tiles.slice = (slice_t){0,0};

        // Restage blast_tiles for the current hover -- pushing attack_range
        // above left it as a fresh empty marker, and entering ATTACK mode
        // shouldn't leave it stale for a hover that was already valid.
        if (game->hover_valid && skill_is_aoe(skill)
                && skill_can_target_area(game->grid, game->entities, active, skill, game->hover)) {
            shift += pathing_ranges_push_blast_tiles(allocator, &game->scratch, &game->pathing, game->grid, game->hover, skill.aoe_radius);
        }

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
// reconciles turn order for every casualty and checks game over exactly
// once (not per-casualty -- game_check_game_over asserts
// game->game_over == GAME_OVER_NONE on entry). Only called by
// game_try_cast_attack_area, which stages game->pathing.blast_tiles for
// `impact` immediately before calling this.
PRIVATE ptrdiff_t game_cast_attack_area(game_state_t *game, linear_allocator_t *allocator, entity_t *active, skill_t skill, position_t impact) {
    assert_debug(SLICE_TYPESIZE(game->pathing.blast_tiles) > 0);
    slice_position_t blast_tiles = game->pathing.blast_tiles;

    // action_try_attack_area requires allocator's cursor pre-aligned to
    // _Alignof(entity_ptr_t) (it does not self-align -- see its doc
    // comment); paired with the pop below so this fully unwinds either way.
    slice_t hit_align = linear_allocator_push_alignment(allocator, _Alignof(entity_ptr_t));

    slice_entity_ptr_t out_hit;
    if (!action_try_attack_area(allocator, game->entities, active, skill, impact, game->pathing.attack_range_tiles, blast_tiles, &out_hit)) {
        linear_allocator_pop(allocator, hit_align);
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

    return game_set_mode(game, allocator, GAME_MODE_MOVEMENT);
}

// Gates `impact` via skill_can_target_area, stages blast_tiles fresh for it
// (a click's impact tile isn't guaranteed to match the last hover tile),
// then casts. Shared by game_on_entity_pressed and game_on_tile_pressed.
PRIVATE ptrdiff_t game_try_cast_attack_area(game_state_t *game, linear_allocator_t *allocator, entity_t *active, skill_t skill, position_t impact) {
    if (!skill_can_target_area(game->grid, game->entities, active, skill, impact)) {
        return 0;
    }
    pathing_ranges_clear_blast_tiles(&game->scratch, &game->pathing);
    ptrdiff_t shift = pathing_ranges_push_blast_tiles(allocator, &game->scratch, &game->pathing, game->grid, impact, skill.aoe_radius);
    return shift + game_cast_attack_area(game, allocator, active, skill, impact);
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
        return game_try_cast_attack_area(game, allocator, active, skill, entity->position);
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
            return game_try_cast_attack_area(game, allocator, active, skill, target);
        }
        return 0;
    }

    if (game->mode != GAME_MODE_MOVEMENT) {
        return 0;
    }

    // game_on_input_event already routed occupied tiles to
    // game_on_entity_pressed, so an entity on `target` here is a dispatch bug.
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
    // game_on_input_event only calls this once these conditions already
    // hold, so they're preconditions here, not runtime no-ops.
    assert_debug(active->team == ENTITY_TEAM_PLAYER);
    assert_debug(game->mode != GAME_MODE_NONE);
    assert_debug(index >= 0);
    assert_debug(index < entity_skill_count(active));

    game->selected_skill = index;
    // game_set_mode's ATTACK branch itself restages blast_tiles at the
    // current hover for whatever skill is now selected, so there's nothing
    // left to do here beyond re-entering the (possibly unchanged) mode.
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

        // mode can only be ATTACK when the active entity is player-controlled
        // (the only callers that set ATTACK mode gate on that first);
        // asserted rather than re-checked since MOUSE_MOVE fires regardless
        // of whose turn it is.
        pathing_ranges_clear_blast_tiles(&game->scratch, &game->pathing);
        if (game->mode == GAME_MODE_ATTACK) {
            entity_t *active = turn_active_entity(game->turn);
            skill_t skill = SLICE_AT(active->skills, game->selected_skill);
            assert_debug(active->team == ENTITY_TEAM_PLAYER);
            if (game->hover_valid && skill_is_aoe(skill)
                    && skill_can_target_area(game->grid, game->entities, active, skill, game->hover)) {
                return pathing_ranges_push_blast_tiles(allocator, &game->scratch, &game->pathing, game->grid, game->hover, skill.aoe_radius);
            }
        }
        return 0;
    }
}
