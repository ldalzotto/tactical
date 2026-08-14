#include "game.h"

#include "../lib/assert.h"
#include "action.h"
#include "ai.h"
#include "pathing.h"
#include "render_cache.h"

// game.c is the one place that combines layout.h's screen geometry with
// entity.h's game data, so it's the right place to enforce the coupling
// layout.h/.c deliberately doesn't know about: viewport_t.skill_buttons and
// entity_t.skills must stay the same length, or render_hud/game_on_input_event
// (which index one array by the other's count) risk an out-of-bounds read.
_Static_assert(VIEWPORT_MAX_SKILL_BUTTONS == ENTITY_MAX_SKILLS, "viewport_t.skill_buttons must have one slot per entity_t.skills slot");

// Fixed size for game->scratch. Hosts reachable_tiles (move range) and
// attack_range_tiles (skill range), mutually exclusive -- whichever is
// off-mode stays nullified (see render_cache_reset), so scratch only ever
// needs room for one populated tile cache.
#define GAME_SCRATCH_CAPACITY 256

PRIVATE void game_check_game_over(game_state_t *game) {
    if (game->game_over != GAME_OVER_NONE) {
        return;
    }

    if (entity_alive_count(game->entities, ENTITY_TEAM_ENEMY) == 0) {
        game->game_over = GAME_OVER_WIN;
        return;
    }

    if (entity_alive_count(game->entities, ENTITY_TEAM_PLAYER) == 0) {
        game->game_over = GAME_OVER_LOSE;
    }
}

PUBLIC game_state_t game_init(linear_allocator_t *allocator, slice_t grid_align, grid_t grid, slice_t entity_list_align, slice_entity_t entities, slice_t turn_order_align, slice_entity_ptr_t turn_order, int fb_width, int fb_height, int hud_height) {
    assert_debug((void*)entities.begin >= (void*)grid.tiles.end);
    assert_debug((void*)turn_order.begin >= (void*)entities.end);

    viewport_t viewport = layout_compute(fb_width, fb_height, grid.width, grid.height, hud_height);

    slice_t scratch_region = linear_allocator_push(allocator, GAME_SCRATCH_CAPACITY);
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
    entity_list_deinit(allocator, state.entities);
    linear_allocator_pop(allocator, state.entity_list_align);
    grid_deinit(allocator, state.grid);
    linear_allocator_pop(allocator, state.grid_align);
}

PRIVATE bool game_tile_is_reachable(pathing_state_t pathing, grid_t grid, position_t position, int mp) {
    int dist = pathing_distance_at(pathing, grid, position);
    return dist > 0 && dist <= mp;
}

// Switches to `mode`. reachable_tiles and attack_range_tiles are mutually
// exclusive, so each branch nullifies the one it isn't populating:
// - NONE: nullifies both -- nothing selected, nothing to show.
// - MOVEMENT: recomputes reachable_tiles for the turn's active entity (empty
//   if it has no mp left), nullifies attack_range_tiles. Called eagerly on
//   any selection/position/mp change, and whenever attack mode turns off.
// - ATTACK: mirror image -- nullifies reachable_tiles, computes
//   attack_range_tiles via the same BFS rooted at the active entity's
//   currently-selected skill range instead of mp.
// Render just reads the cached lists, no per-frame pathing.
PRIVATE void game_set_mode(game_state_t *game, linear_allocator_t *allocator, game_mode_t mode) {
    render_cache_reset(&game->scratch, &game->render);
    game->mode = mode;

    if (mode == GAME_MODE_NONE) {
        return;
    }

    entity_t *active = turn_active_entity(game->turn);

    if (mode == GAME_MODE_MOVEMENT) {
        if (active->mp <= 0) {
            return;
        }

        pathing_state_t pathing = pathing_compute_distances(allocator, game->grid, game->entities, active, active->position, active->mp, 0);

        slice_t reachable_align = linear_allocator_push_alignment(&game->scratch, _Alignof(position_t));
        slice_position_t reachable_tiles = LINEAR_ALLOCATOR_PUSH(&game->scratch, game->render.reachable_tiles, 0);
        for (int ty = 0; ty < game->grid.height; ty++) {
            for (int tx = 0; tx < game->grid.width; tx++) {
                position_t position = { tx, ty };
                if (game_tile_is_reachable(pathing, game->grid, position, active->mp)) {
                    slice_position_t entry = LINEAR_ALLOCATOR_PUSH(&game->scratch, game->render.reachable_tiles, 1);
                    SLICE_DEREF(entry) = position;
                    reachable_tiles.end = entry.end;
                }
            }
        }

        render_cache_write_reachable(&game->scratch, &game->render, reachable_align, reachable_tiles);

        pathing_deinit(allocator, pathing);
        return;
    } else if (mode == GAME_MODE_ATTACK) {
        int skill_range = active->skills[game->selected_skill].range;
        // pass_through_opposing_team_of=active: attack-range preview treats
        // other enemies as passable so tiles behind them stay reachable-for-
        // targeting -- see ticket 003 / PLAN.md Q1-Q2.
        pathing_state_t pathing = pathing_compute_distances(allocator, game->grid, game->entities, active, active->position, skill_range, active);

        slice_t attack_range_align = linear_allocator_push_alignment(&game->scratch, _Alignof(position_t));
        slice_position_t attack_range_tiles = LINEAR_ALLOCATOR_PUSH(&game->scratch, game->render.attack_range_tiles, 0);
        for (int ty = 0; ty < game->grid.height; ty++) {
            for (int tx = 0; tx < game->grid.width; tx++) {
                position_t position = { tx, ty };
                if (game_tile_is_reachable(pathing, game->grid, position, skill_range)) {
                    slice_position_t entry = LINEAR_ALLOCATOR_PUSH(&game->scratch, game->render.attack_range_tiles, 1);
                    SLICE_DEREF(entry) = position;
                    attack_range_tiles.end = entry.end;
                }
            }
        }

        render_cache_write_attack_range(&game->render, attack_range_align, attack_range_tiles);

        pathing_deinit(allocator, pathing);
    }
}

// Advances the cursor past the entity that just finished acting, then lets
// the AI play out every consecutive enemy turn until either a player entity
// becomes active or the game ends.
PRIVATE void game_advance_turn(game_state_t *game, linear_allocator_t *allocator) {
    game->turn = turn_advance(game->turn);
    game->selected_skill = 0;
    game_set_mode(game, allocator, GAME_MODE_NONE);

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
}

PRIVATE void game_on_entity_pressed(game_state_t *game, linear_allocator_t *allocator, entity_t* entity) {
    assert_debug(game->game_over == GAME_OVER_NONE);
    assert_debug(entity != 0);
    assert_debug(entity->alive);

    entity_t *active = turn_active_entity(game->turn);
    if (active->team != ENTITY_TEAM_PLAYER) {
        return;
    }

    entity_t *pressed = entity;
    if (pressed == active) {
        game_set_mode(game, allocator, GAME_MODE_MOVEMENT);
        return;
    }

    if (game->mode != GAME_MODE_ATTACK) {
        return;
    }

    if (action_try_attack(allocator, game->grid, game->entities, active, active->skills[game->selected_skill], entity)) {
        // If the entity just died, we remove dead entities
        if (!entity->alive) {
            game->turn = turn_remove_dead_entities(game->turn);
        }
        game_check_game_over(game);
        game_set_mode(game, allocator, GAME_MODE_MOVEMENT);
    }
}

PRIVATE void game_on_tile_pressed(game_state_t *game, linear_allocator_t *allocator, position_t target) {
    assert_debug(game->game_over == GAME_OVER_NONE);
    entity_t *active = turn_active_entity(game->turn);
    if (active->team != ENTITY_TEAM_PLAYER) {
        return;
    }
    if (game->mode == GAME_MODE_NONE) {
        return;
    }

    if (entity_find_at(game->entities, target) != 0) {
        return;
    }

    if (action_try_move(allocator, game->grid, game->entities, active, target)) {
        game_set_mode(game, allocator, GAME_MODE_MOVEMENT);
    }
}

PRIVATE void game_on_attack_toggle_pressed(game_state_t *game, linear_allocator_t *allocator) {
    assert_debug(game->game_over == GAME_OVER_NONE);
    entity_t *active = turn_active_entity(game->turn);
    if (active->team != ENTITY_TEAM_PLAYER) {
        return;
    }
    if (game->mode == GAME_MODE_NONE) {
        return;
    }

    game_set_mode(game, allocator, game->mode == GAME_MODE_ATTACK ? GAME_MODE_MOVEMENT : GAME_MODE_ATTACK);
}

// Selects entity_t.skills[index] as the player's active skill (game->selected_skill)
// and recomputes the range preview for it. No-op outside player control / no
// active mode / an index the entity doesn't have that many skills for.
// Safe to call repeatedly (including with the same mode already active) --
// game_set_mode's render_cache_reset always pops back to the pre-selection
// watermark before re-pushing, so this can't leak/overflow game->scratch.
PRIVATE void game_on_skill_button_pressed(game_state_t *game, linear_allocator_t *allocator, int index) {
    assert_debug(game->game_over == GAME_OVER_NONE);
    entity_t *active = turn_active_entity(game->turn);
    if (active->team != ENTITY_TEAM_PLAYER) {
        return;
    }
    if (game->mode == GAME_MODE_NONE) {
        return;
    }
    if (index < 0 || index >= active->skill_count) {
        return;
    }

    game->selected_skill = index;
    game_set_mode(game, allocator, game->mode);
}

PRIVATE void game_on_end_turn_pressed(game_state_t *game, linear_allocator_t *allocator) {
    assert_debug(game->game_over == GAME_OVER_NONE);
    entity_t *active = turn_active_entity(game->turn);
    if (active->team != ENTITY_TEAM_PLAYER) {
        return;
    }

    game_advance_turn(game, allocator);
}

PUBLIC void game_on_input_event(game_state_t *game, linear_allocator_t *allocator, input_event_t event) {
    if (game->game_over != GAME_OVER_NONE) {
        return;
    }

    if (event.type == INPUT_EVENT_MOUSE_CLICK) {
        if (point_in_rect(game->viewport.end_turn_button, event.x, event.y)) {
            game_on_end_turn_pressed(game, allocator);
            return;
        }

        if (point_in_rect(game->viewport.attack_button, event.x, event.y)) {
            game_on_attack_toggle_pressed(game, allocator);
            return;
        }

        // Only hit-test skill buttons when render_hud would actually be
        // drawing them (same gate as there): otherwise nothing occupies
        // that screen region and a click there should fall through to the
        // grid underneath it instead of being silently swallowed.
        entity_t *active_for_skill_buttons = turn_active_entity(game->turn);
        if (active_for_skill_buttons->team == ENTITY_TEAM_PLAYER && game->mode != GAME_MODE_NONE && active_for_skill_buttons->skill_count > 1) {
            for (int i = 0; i < active_for_skill_buttons->skill_count; i++) {
                if (point_in_rect(game->viewport.skill_buttons[i], event.x, event.y)) {
                    game_on_skill_button_pressed(game, allocator, i);
                    return;
                }
            }
        }

        int tx, ty;
        if (!screen_to_grid(game->viewport, event.x, event.y, &tx, &ty)) {
            return;
        }

        position_t target = { tx, ty };
        entity_t* found = entity_find_at(game->entities, target);
        if (found != 0) {
            game_on_entity_pressed(game, allocator, found);
        } else {
            game_on_tile_pressed(game, allocator, target);
        }
    } else if (event.type == INPUT_EVENT_MOUSE_MOVE) {
        int tx, ty;
        bool valid = screen_to_grid(game->viewport, event.x, event.y, &tx, &ty);
        game->hover_valid = valid;
        if (valid) {
            game->hover = (position_t){ tx, ty };
        }
    }
}
