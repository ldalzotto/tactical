#include "render.h"

// Colors -- see the T12 ticket's drawing spec for the exact values below;
// anything not explicitly pinned there (the "lighter" gridline fill, entity
// square/HP-bar insets, HUD background, AP/MP indicator sizing) is a
// reasonable implementation choice, not a spec value.
static const rgba_t COLOR_TILE_WALKABLE = { 40, 40, 40, 255 };
static const rgba_t COLOR_TILE_WALKABLE_INSET = { 60, 60, 60, 255 };
static const rgba_t COLOR_TILE_OBSTACLE = { 10, 10, 10, 255 };
static const rgba_t COLOR_TILE_OBSTACLE_INSET = { 30, 30, 30, 255 };
static const rgba_t COLOR_REACHABLE_TINT = { 80, 140, 220, 255 };
static const rgba_t COLOR_WHITE = { 255, 255, 255, 255 };
static const rgba_t COLOR_PLAYER = { 60, 120, 255, 255 };
static const rgba_t COLOR_ENEMY = { 220, 60, 60, 255 };
static const rgba_t COLOR_HP_BG = { 120, 20, 20, 255 };
static const rgba_t COLOR_HP_FG = { 60, 200, 60, 255 };
static const rgba_t COLOR_HUD_BG = { 20, 20, 20, 255 };
static const rgba_t COLOR_END_TURN_ACTIVE = { 60, 200, 60, 255 };
static const rgba_t COLOR_END_TURN_INACTIVE = { 80, 80, 80, 255 };
static const rgba_t COLOR_AP_PIP = { 60, 120, 255, 255 };
static const rgba_t COLOR_MP_PIP = { 60, 200, 60, 255 };
static const rgba_t COLOR_WIN = { 0, 200, 0, 255 };
static const rgba_t COLOR_LOSE = { 200, 0, 0, 255 };

#define OUTLINE_THICKNESS 2

// Draws a 2px-thick white rectangular outline as 4 thin edge rects, since
// graphics.h has no dedicated outline/stroke primitive.
static void render_draw_outline(slice_rgba_t fb, int fb_width, int x, int y, int width, int height, rgba_t color) {
    graphics_draw_rectangle(fb, fb_width, x, y, width, OUTLINE_THICKNESS, color);
    graphics_draw_rectangle(fb, fb_width, x, y + height - OUTLINE_THICKNESS, width, OUTLINE_THICKNESS, color);
    graphics_draw_rectangle(fb, fb_width, x, y, OUTLINE_THICKNESS, height, color);
    graphics_draw_rectangle(fb, fb_width, x + width - OUTLINE_THICKNESS, y, OUTLINE_THICKNESS, height, color);
}

static void render_tiles(slice_rgba_t fb, int fb_width, game_state_t game) {
    bool has_reachable_overlay = game.selected_entity != ENTITY_ID_NONE;
    entity_t *selected = has_reachable_overlay ? entity_at(game.entities, game.selected_entity) : 0;
    has_reachable_overlay = has_reachable_overlay && selected->mp > 0;

    if (has_reachable_overlay) {
        pathing_compute_distances(game.pathing, game.grid, game.entities, game.selected_entity, selected->x, selected->y, selected->mp);
    }

    for (int ty = 0; ty < game.grid.height; ty++) {
        for (int tx = 0; tx < game.grid.width; tx++) {
            bool walkable = grid_is_walkable(game.grid, tx, ty);
            rgba_t outer = walkable ? COLOR_TILE_WALKABLE : COLOR_TILE_OBSTACLE;
            rgba_t inset = walkable ? COLOR_TILE_WALKABLE_INSET : COLOR_TILE_OBSTACLE_INSET;

            int px, py;
            grid_to_screen(game.viewport, tx, ty, &px, &py);
            int ts = game.viewport.tile_size;

            graphics_draw_rectangle(fb, fb_width, px, py, ts, ts, outer);
            graphics_draw_rectangle(fb, fb_width, px + 2, py + 2, ts - 4, ts - 4, inset);

            if (has_reachable_overlay) {
                int dist = pathing_distance_at(game.pathing, game.grid, tx, ty);
                if (dist > 0 && dist <= selected->mp) {
                    graphics_draw_rectangle(fb, fb_width, px, py, ts, ts, COLOR_REACHABLE_TINT);
                }
            }
        }
    }

    if (game.hover_valid) {
        int px, py;
        grid_to_screen(game.viewport, game.hover_x, game.hover_y, &px, &py);
        int ts = game.viewport.tile_size;
        render_draw_outline(fb, fb_width, px, py, ts, ts, COLOR_WHITE);
    }
}

static void render_hp_bar(slice_rgba_t fb, int fb_width, int px, int py, int ts, entity_t *entity) {
    int margin = ts / 6;
    if (margin < 1) margin = 1;
    int bar_height = ts / 8;
    if (bar_height < 2) bar_height = 2;

    int bar_x = px + margin;
    int bar_y = py + 2;
    int bar_width = ts - 2 * margin;
    if (bar_width < 1) bar_width = 1;

    graphics_draw_rectangle(fb, fb_width, bar_x, bar_y, bar_width, bar_height, COLOR_HP_BG);

    int fg_width = entity->max_hp > 0 ? (bar_width * entity->hp) / entity->max_hp : 0;
    if (fg_width < 0) fg_width = 0;
    if (fg_width > bar_width) fg_width = bar_width;
    graphics_draw_rectangle(fb, fb_width, bar_x, bar_y, fg_width, bar_height, COLOR_HP_FG);
}

static void render_entities(slice_rgba_t fb, int fb_width, game_state_t game) {
    for (int i = 0; i < game.entities.count; i++) {
        entity_t *entity = entity_at(game.entities, (entity_id_t)i);
        if (!entity->alive) {
            continue;
        }

        int px, py;
        grid_to_screen(game.viewport, entity->x, entity->y, &px, &py);
        int ts = game.viewport.tile_size;

        render_hp_bar(fb, fb_width, px, py, ts, entity);

        int margin = ts / 6;
        if (margin < 1) margin = 1;
        int square_top = py + 2 + (ts / 8 < 2 ? 2 : ts / 8) + 2;
        int square_x = px + margin;
        int square_width = ts - 2 * margin;
        int square_height = py + ts - margin - square_top;
        if (square_width < 1) square_width = 1;
        if (square_height < 1) square_height = 1;

        rgba_t color = entity->team == ENTITY_TEAM_PLAYER ? COLOR_PLAYER : COLOR_ENEMY;
        graphics_draw_rectangle(fb, fb_width, square_x, square_top, square_width, square_height, color);

        if ((entity_id_t)i == game.selected_entity) {
            render_draw_outline(fb, fb_width, px, py, ts, ts, COLOR_WHITE);
        }
    }
}

static void render_hud(slice_rgba_t fb, int fb_width, game_state_t game) {
    rect_t hud = game.viewport.hud_rect;
    graphics_draw_rectangle(fb, fb_width, hud.x, hud.y, hud.width, hud.height, COLOR_HUD_BG);

    rect_t button = game.viewport.end_turn_button;
    rgba_t button_color = game.turn.phase == TURN_PHASE_PLAYER ? COLOR_END_TURN_ACTIVE : COLOR_END_TURN_INACTIVE;
    graphics_draw_rectangle(fb, fb_width, button.x, button.y, button.width, button.height, button_color);

    if (game.selected_entity == ENTITY_ID_NONE) {
        return;
    }
    entity_t *selected = entity_at(game.entities, game.selected_entity);

    int pip_size = 10;
    int pip_gap = 4;
    int row_gap = 6;
    int start_x = hud.x + 10;
    int ap_y = hud.y + (hud.height - (2 * pip_size + row_gap)) / 2;
    int mp_y = ap_y + pip_size + row_gap;

    for (int i = 0; i < selected->ap; i++) {
        int x = start_x + i * (pip_size + pip_gap);
        graphics_draw_rectangle(fb, fb_width, x, ap_y, pip_size, pip_size, COLOR_AP_PIP);
    }

    for (int i = 0; i < selected->mp; i++) {
        int x = start_x + i * (pip_size + pip_gap);
        graphics_draw_rectangle(fb, fb_width, x, mp_y, pip_size, pip_size, COLOR_MP_PIP);
    }
}

void render_frame(slice_rgba_t framebuffer, int fb_width, game_state_t game) {
    if (game.game_over != GAME_OVER_NONE) {
        int pixel_count = (int)(framebuffer.end - framebuffer.begin);
        int fb_height = fb_width > 0 ? pixel_count / fb_width : 0;
        rgba_t fill = game.game_over == GAME_OVER_WIN ? COLOR_WIN : COLOR_LOSE;
        graphics_draw_rectangle(framebuffer, fb_width, 0, 0, fb_width, fb_height, fill);
        return;
    }

    render_tiles(framebuffer, fb_width, game);
    render_entities(framebuffer, fb_width, game);
    render_hud(framebuffer, fb_width, game);
}
