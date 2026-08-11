#include "game.h"

#include "../lib/assert.h"
#include "action.h"
#include "ai.h"

static void game_check_game_over(game_state_t *game) {
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

game_state_t game_init(slice_t grid_align, grid_t grid, slice_t entity_list_align, slice_entity_t entities, slice_t turn_order_align, slice_entity_ptr_t turn_order, int fb_width, int fb_height, int hud_height) {
    assert_debug((void*)entities.begin >= (void*)grid.tiles.end);
    assert_debug((void*)turn_order.begin >= (void*)entities.end);

    viewport_t viewport = layout_compute(fb_width, fb_height, grid.width, grid.height, hud_height);

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
    };
    return game;
}

void game_deinit(linear_allocator_t *allocator, game_state_t state) {
    turn_order_deinit(allocator, state.turn.capacity);
    linear_allocator_pop(allocator, state.turn_order_align);
    entity_list_deinit(allocator, state.entities);
    linear_allocator_pop(allocator, state.entity_list_align);
    grid_deinit(allocator, state.grid);
    linear_allocator_pop(allocator, state.grid_align);
}

// Advances the cursor past the entity that just finished acting, then lets
// the AI play out every consecutive enemy turn until either a player entity
// becomes active or the game ends.
static void game_advance_turn(game_state_t *game, linear_allocator_t *allocator) {
    game->turn = turn_advance(game->turn);
    game->selected_entity = 0;

    entity_t *active = turn_active_entity(game->turn);
    while (game->game_over == GAME_OVER_NONE && active->team == ENTITY_TEAM_ENEMY) {
        ai_run_ennemy_turn(allocator, game->grid, game->entities, active);
        // TODO: should turn_remove_dead_entities and game_check_game_over
        // be called conditionally based on some return of ai_run_ennemy_turn.
        game->turn = turn_remove_dead_entities(game->turn);
        game_check_game_over(game);

        game->turn = turn_advance(game->turn);
        active = turn_active_entity(game->turn);
    }
}

void game_on_entity_pressed(game_state_t *game, entity_t* entity) {
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
        return;
    }

    if (game->selected_entity == 0) {
        return;
    }

    entity_t *selected = game->selected_entity;
    if (!entity_is_adjacent(*selected, *pressed)) {
        return;
    }
    if (selected->ap <= 0) {
        return;
    }

    if (action_try_attack(game->selected_entity, entity)) {
        // If the entity just died, we remove dead entities
        if (!entity->alive) {
            game->turn = turn_remove_dead_entities(game->turn);
        }
        game_check_game_over(game);
    }
}

void game_on_tile_pressed(game_state_t *game, linear_allocator_t *allocator, position_t target) {
    // TODO: we have to be able to assert_debug(game->game_over == GAME_OVER_NONE);
    if (game->game_over != GAME_OVER_NONE) {
        return;
    }
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

    action_try_move(allocator, game->grid, game->entities, game->selected_entity, target);
}

void game_on_end_turn_pressed(game_state_t *game, linear_allocator_t *allocator) {
    if (game->game_over != GAME_OVER_NONE) {
        return;
    }
    entity_t *active = turn_active_entity(game->turn);
    if (active->team != ENTITY_TEAM_PLAYER) {
        return;
    }

    game_advance_turn(game, allocator);
}

void game_on_input_event(game_state_t *game, linear_allocator_t *allocator, input_event_t event) {
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
            game_on_entity_pressed(game, found);
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
