#include "game.h"

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

game_state_t game_init(linear_allocator_t *allocator, int grid_width, int grid_height, int fb_width, int fb_height, int hud_height) {
    grid_t grid = grid_init(allocator, grid_width, grid_height);

    linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    entity_list_t entities = entity_list_init(allocator, GAME_MAX_ENTITIES);

    linear_allocator_push_alignment(allocator, _Alignof(int32_t));
    pathing_state_t pathing = pathing_init(allocator, grid_width, grid_height);

    viewport_t viewport = layout_compute(fb_width, fb_height, grid_width, grid_height, hud_height);

    game_state_t game = {
        .grid = grid,
        .entities = entities,
        .pathing = pathing,
        .turn = turn_init(),
        .viewport = viewport,
        .selected_entity = ENTITY_ID_NONE,
        .hover_x = 0,
        .hover_y = 0,
        .hover_valid = false,
        .game_over = GAME_OVER_NONE,
    };
    return game;
}

void game_deinit(linear_allocator_t *allocator, game_state_t state) {
    pathing_deinit(allocator, state.pathing);

    // Padding pushed between the entities allocation and the pathing
    // allocation sits exactly between entities.entities.end and
    // pathing.dist.begin (see game_init's push order).
    slice_t pathing_align_marker = { state.entities.entities.end, state.pathing.dist.begin };
    linear_allocator_pop(allocator, pathing_align_marker);

    entity_list_deinit(allocator, state.entities);

    // Same reasoning for the padding pushed between the grid allocation and
    // the entities allocation.
    slice_t entity_align_marker = { state.grid.tiles.end, state.entities.entities.begin };
    linear_allocator_pop(allocator, entity_align_marker);

    grid_deinit(allocator, state.grid);
}

void game_on_entity_pressed(game_state_t *game, entity_id_t entity) {
    if (game->game_over != GAME_OVER_NONE) {
        return;
    }
    if (game->turn.phase != TURN_PHASE_PLAYER) {
        return;
    }
    if (entity == ENTITY_ID_NONE) {
        return;
    }

    entity_t *pressed = entity_at(game->entities, entity);
    if (!pressed->alive) {
        return;
    }

    if (pressed->team == ENTITY_TEAM_PLAYER) {
        game->selected_entity = entity;
        return;
    }

    if (game->selected_entity == ENTITY_ID_NONE) {
        return;
    }

    entity_t *selected = entity_at(game->entities, game->selected_entity);
    if (!entity_is_adjacent(*selected, *pressed)) {
        return;
    }
    if (selected->ap <= 0) {
        return;
    }

    if (action_try_attack(game->entities, game->selected_entity, entity)) {
        game_check_game_over(game);
    }
}

void game_on_tile_pressed(game_state_t *game, int tx, int ty) {
    if (game->game_over != GAME_OVER_NONE) {
        return;
    }
    if (game->turn.phase != TURN_PHASE_PLAYER) {
        return;
    }
    if (game->selected_entity == ENTITY_ID_NONE) {
        return;
    }

    if (entity_find_at(game->entities, tx, ty) != ENTITY_ID_NONE) {
        return;
    }

    action_try_move(game->grid, game->entities, game->pathing, game->selected_entity, tx, ty);
}

void game_on_end_turn_pressed(game_state_t *game) {
    if (game->game_over != GAME_OVER_NONE) {
        return;
    }
    if (game->turn.phase != TURN_PHASE_PLAYER) {
        return;
    }

    game->turn = turn_begin_enemy_phase(game->turn, game->entities);
    game->selected_entity = ENTITY_ID_NONE;

    ai_run_enemy_phase(game->grid, game->entities, game->pathing);

    game_check_game_over(game);

    if (game->game_over == GAME_OVER_NONE) {
        game->turn = turn_begin_player_phase(game->turn, game->entities);
    }
}

void game_on_input_event(game_state_t *game, input_event_t event) {
    if (game->game_over != GAME_OVER_NONE) {
        return;
    }

    if (event.type == INPUT_EVENT_MOUSE_CLICK) {
        if (point_in_rect(game->viewport.end_turn_button, event.x, event.y)) {
            game_on_end_turn_pressed(game);
            return;
        }

        int tx, ty;
        if (!screen_to_grid(game->viewport, event.x, event.y, &tx, &ty)) {
            return;
        }

        entity_id_t found = entity_find_at(game->entities, tx, ty);
        if (found != ENTITY_ID_NONE) {
            game_on_entity_pressed(game, found);
        } else {
            game_on_tile_pressed(game, tx, ty);
        }
    } else if (event.type == INPUT_EVENT_MOUSE_MOVE) {
        int tx, ty;
        bool valid = screen_to_grid(game->viewport, event.x, event.y, &tx, &ty);
        game->hover_valid = valid;
        if (valid) {
            game->hover_x = tx;
            game->hover_y = ty;
        }
    }
}
