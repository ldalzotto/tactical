#include "game.h"

#include "../lib/assert.h"
#include "action.h"
#include "ai.h"
#include "pathing.h"

// Fixed size for game->scratch, currently only hosting reachable_tiles.
// In the future, grow/shrink this region on demand instead of a flat guess.
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

    game_state_t game = {
        .grid_align = grid_align,
        .grid = grid,
        .entity_list_align = entity_list_align,
        .entities = entities,
        .turn_order_align = turn_order_align,
        .turn = turn_init(turn_order),
        .viewport = viewport,
        .selected_entity = 0,
        .hover = { 0, 0 },
        .hover_valid = false,
        .game_over = GAME_OVER_NONE,
        .scratch = scratch,
        .render = {
            .reachable_align = reachable_align,
            .reachable_tiles = reachable_tiles,
        },
    };
    return game;
}

PUBLIC void game_deinit(linear_allocator_t *allocator, game_state_t state) {
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

// Recomputes the selected entity's reachable-tiles cache from scratch.
// Called eagerly whenever selection, position, or mp of the selected entity
// changes -- render just reads the cached list, no per-frame pathing.
// Pushes/pops game->scratch on the fly: pops the previous computation (tiles,
// then its alignment padding if any), then pushes exactly as many tiles as
// the new one needs (counted in a first pass over the grid, filled in a
// second), aligning right before that push -- so no alignment padding sits
// in scratch while nothing is selected.
PRIVATE void game_refresh_reachable_render(game_state_t *game, linear_allocator_t *allocator) {
    LINEAR_ALLOCATOR_POP(&game->scratch, game->render.reachable_tiles);
    linear_allocator_pop(&game->scratch, game->render.reachable_align);

    entity_t *selected = game->selected_entity;
    if (selected == 0 || selected->mp <= 0) {
        game->render.reachable_align = linear_allocator_push(&game->scratch, 0);
        game->render.reachable_tiles = LINEAR_ALLOCATOR_PUSH(&game->scratch, game->render.reachable_tiles, 0);
        return;
    }

    pathing_state_t pathing = pathing_compute_distances(allocator, game->grid, game->entities, selected, selected->position, selected->mp);

    int count = 0;
    for (int ty = 0; ty < game->grid.height; ty++) {
        for (int tx = 0; tx < game->grid.width; tx++) {
            if (game_tile_is_reachable(pathing, game->grid, (position_t){tx, ty}, selected->mp)) {
                count++;
            }
        }
    }

    game->render.reachable_align = linear_allocator_push_alignment(&game->scratch, _Alignof(position_t));
    game->render.reachable_tiles = LINEAR_ALLOCATOR_PUSH(&game->scratch, game->render.reachable_tiles, count);

    int i = 0;
    for (int ty = 0; ty < game->grid.height; ty++) {
        for (int tx = 0; tx < game->grid.width; tx++) {
            position_t position = { tx, ty };
            if (game_tile_is_reachable(pathing, game->grid, position, selected->mp)) {
                SLICE_AT(game->render.reachable_tiles, i) = position;
                i++;
            }
        }
    }

    pathing_deinit(allocator, pathing);
}

// Advances the cursor past the entity that just finished acting, then lets
// the AI play out every consecutive enemy turn until either a player entity
// becomes active or the game ends.
PRIVATE void game_advance_turn(game_state_t *game, linear_allocator_t *allocator) {
    game->turn = turn_advance(game->turn);
    game->selected_entity = 0;
    game_refresh_reachable_render(game, allocator);

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
        game->selected_entity = entity;
        game_refresh_reachable_render(game, allocator);
        return;
    }

    if (game->selected_entity == 0) {
        return;
    }

    if (action_try_attack(allocator, game->grid, game->entities, game->selected_entity, entity)) {
        // If the entity just died, we remove dead entities
        if (!entity->alive) {
            game->turn = turn_remove_dead_entities(game->turn);
        }
        game_check_game_over(game);
    }
}

PRIVATE void game_on_tile_pressed(game_state_t *game, linear_allocator_t *allocator, position_t target) {
    assert_debug(game->game_over == GAME_OVER_NONE);
    entity_t *active = turn_active_entity(game->turn);
    if (active->team != ENTITY_TEAM_PLAYER) {
        return;
    }
    if (game->selected_entity == 0) {
        return;
    }

    if (entity_find_at(game->entities, target) != 0) {
        return;
    }

    if (action_try_move(allocator, game->grid, game->entities, game->selected_entity, target)) {
        game_refresh_reachable_render(game, allocator);
    }
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
