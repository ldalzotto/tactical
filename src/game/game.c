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

game_state_t game_init(slice_t grid_align, grid_t grid, slice_t entity_list_align, slice_entity_t entities, int fb_width, int fb_height, int hud_height) {
    assert_debug((void*)entities.begin >= (void*)grid.tiles.end);

    viewport_t viewport = layout_compute(fb_width, fb_height, grid.width, grid.height, hud_height);

    game_state_t game = {
        .grid_align = grid_align,
        .grid = grid,
        .entity_list_align = entity_list_align,
        .entities = entities,
        .turn = turn_init(),
        .viewport = viewport,
        .selected_entity = 0,
        .hover = { 0, 0 },
        .hover_valid = false,
        .game_over = GAME_OVER_NONE,
    };
    return game;
}

void game_deinit(linear_allocator_t *allocator, game_state_t state) {
    entity_list_deinit(allocator, state.entities);
    linear_allocator_pop(allocator, state.entity_list_align);
    grid_deinit(allocator, state.grid);
    linear_allocator_pop(allocator, state.grid_align);
}

void game_on_entity_pressed(game_state_t *game, entity_t* entity) {
    if (game->game_over != GAME_OVER_NONE) {
        return;
    }
    if (game->turn.phase != TURN_PHASE_PLAYER) {
        return;
    }
    if (entity == 0) {
        return;
    }

    entity_t *pressed = entity;
    if (!pressed->alive) {
        return;
    }

    if (pressed->team == ENTITY_TEAM_PLAYER) {
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
        game_check_game_over(game);
    }
}

void game_on_tile_pressed(game_state_t *game, linear_allocator_t *allocator, position_t target) {
    if (game->game_over != GAME_OVER_NONE) {
        return;
    }
    if (game->turn.phase != TURN_PHASE_PLAYER) {
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
    if (game->turn.phase != TURN_PHASE_PLAYER) {
        return;
    }

    game->turn = turn_begin_enemy_phase(game->turn, game->entities);
    game->selected_entity = 0;

    ai_run_enemy_phase(allocator, game->grid, game->entities);

    game_check_game_over(game);

    if (game->game_over == GAME_OVER_NONE) {
        game->turn = turn_begin_player_phase(game->turn, game->entities);
    }
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
