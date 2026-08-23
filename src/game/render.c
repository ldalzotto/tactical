#include "render.h"

#include "../lib/assert.h"

// Colors. Some values (the "lighter" gridline fill, entity square/HP-bar
// insets, HUD background, AP/MP indicator sizing) are implementation
// choices rather than pinned spec values.
static const rgba_t COLOR_TILE_WALKABLE = { 40, 40, 40, 255 };
static const rgba_t COLOR_TILE_WALKABLE_INSET = { 60, 60, 60, 255 };
static const rgba_t COLOR_TILE_OBSTACLE = { 10, 10, 10, 255 };
static const rgba_t COLOR_TILE_OBSTACLE_INSET = { 30, 30, 30, 255 };
// Walkable tiles that still block line of sight (e.g. tall grass): distinct
// from both plain floor and a non-walkable wall.
static const rgba_t COLOR_TILE_WALKABLE_BLOCKS_SIGHT = { 40, 60, 30, 255 };
static const rgba_t COLOR_TILE_WALKABLE_BLOCKS_SIGHT_INSET = { 60, 90, 45, 255 };
// Chasms: not walkable but sight-clear.
static const rgba_t COLOR_TILE_CHASM = { 20, 30, 55, 255 };
static const rgba_t COLOR_TILE_CHASM_INSET = { 30, 45, 80, 255 };
static const rgba_t COLOR_REACHABLE_TINT = { 80, 140, 220, 255 };
static const rgba_t COLOR_ATTACK_RANGE_TINT = { 230, 140, 60, 255 };
// Distinct from COLOR_ATTACK_RANGE_TINT so "what this hit would do" (the
// hovered blast footprint) reads differently from "where I can target".
static const rgba_t COLOR_BLAST_PREVIEW_TINT = { 220, 40, 40, 255 };
static const rgba_t COLOR_WHITE = { 255, 255, 255, 255 };
// Always-on "whose turn it is" marker; distinct from COLOR_WHITE so it
// doesn't collide with the mode-gated selection outline.
static const rgba_t COLOR_TURN_INDICATOR = { 250, 210, 40, 255 };
static const rgba_t COLOR_PLAYER = { 60, 120, 255, 255 };
static const rgba_t COLOR_ENEMY = { 220, 60, 60, 255 };
static const rgba_t COLOR_HP_BG = { 120, 20, 20, 255 };
static const rgba_t COLOR_HP_FG = { 60, 200, 60, 255 };
static const rgba_t COLOR_HUD_BG = { 20, 20, 20, 255 };
static const rgba_t COLOR_END_TURN_ACTIVE = { 60, 200, 60, 255 };
static const rgba_t COLOR_END_TURN_INACTIVE = { 80, 80, 80, 255 };
static const rgba_t COLOR_ATTACK_BUTTON_ON = { 220, 60, 60, 255 };
static const rgba_t COLOR_ATTACK_BUTTON_AVAILABLE = { 230, 140, 60, 255 };
static const rgba_t COLOR_ATTACK_BUTTON_INACTIVE = { 80, 80, 80, 255 };
// Same value as COLOR_ATTACK_BUTTON_AVAILABLE: intentional echo, both mark
// "the active choice".
static const rgba_t COLOR_SKILL_BUTTON_SELECTED = { 230, 140, 60, 255 };
static const rgba_t COLOR_SKILL_BUTTON_AVAILABLE = { 90, 90, 110, 255 };
static const rgba_t COLOR_AP_PIP = { 60, 120, 255, 255 };
static const rgba_t COLOR_MP_PIP = { 60, 200, 60, 255 };
static const rgba_t COLOR_WIN = { 0, 200, 0, 255 };
static const rgba_t COLOR_LOSE = { 200, 0, 0, 255 };

#define OUTLINE_THICKNESS 2
// Small square, inset past the OUTLINE_THICKNESS-px selection outline.
#define TURN_INDICATOR_SIZE 6
#define TURN_INDICATOR_INSET (OUTLINE_THICKNESS + 2)

// Draws a 2px-thick white rectangular outline as 4 thin edge rects, since
// graphics.h has no dedicated outline/stroke primitive.
PRIVATE void render_draw_outline(slice_rgba_t fb, int fb_width, rect_t r, rgba_t color) {
    graphics_draw_rectangle(fb, fb_width, r.x, r.y, r.width, OUTLINE_THICKNESS, color);
    graphics_draw_rectangle(fb, fb_width, r.x, r.y + r.height - OUTLINE_THICKNESS, r.width, OUTLINE_THICKNESS, color);
    graphics_draw_rectangle(fb, fb_width, r.x, r.y, OUTLINE_THICKNESS, r.height, color);
    graphics_draw_rectangle(fb, fb_width, r.x + r.width - OUTLINE_THICKNESS, r.y, OUTLINE_THICKNESS, r.height, color);
}

PRIVATE void render_tiles(slice_rgba_t fb, int fb_width, game_state_t game) {
    for (int ty = 0; ty < game.grid.height; ty++) {
        for (int tx = 0; tx < game.grid.width; tx++) {
            tile_kind_t kind = grid_tile_kind(game.grid, (position_t){tx, ty});
            rgba_t outer, inset;
            if (kind == TILE_WALL) {
                outer = COLOR_TILE_OBSTACLE;
                inset = COLOR_TILE_OBSTACLE_INSET;
            } else if (kind == TILE_GRASS) {
                outer = COLOR_TILE_WALKABLE_BLOCKS_SIGHT;
                inset = COLOR_TILE_WALKABLE_BLOCKS_SIGHT_INSET;
            } else if (kind == TILE_CHASM) {
                outer = COLOR_TILE_CHASM;
                inset = COLOR_TILE_CHASM_INSET;
            } else {
                outer = COLOR_TILE_WALKABLE;
                inset = COLOR_TILE_WALKABLE_INSET;
            }

            int px, py;
            grid_to_screen(game.viewport, tx, ty, &px, &py);
            int ts = game.viewport.tile_size;

            graphics_draw_rectangle(fb, fb_width, px, py, ts, ts, outer);
            graphics_draw_rectangle(fb, fb_width, px + 2, py + 2, ts - 4, ts - 4, inset);
        }
    }

    for (SLICE_FOREACH(game.pathing.reachable_tiles, tile_s)) {
        position_t tile = SLICE_DEREF(tile_s);
        int px, py;
        grid_to_screen(game.viewport, tile.x, tile.y, &px, &py);
        int ts = game.viewport.tile_size;
        graphics_draw_rectangle(fb, fb_width, px, py, ts, ts, COLOR_REACHABLE_TINT);
    }

    entity_t *attacker = turn_active_entity(game.turn);
    for (SLICE_FOREACH(game.pathing.attack_range_tiles, tile_s)) {
        position_t tile = SLICE_DEREF(tile_s);
        int px, py;
        grid_to_screen(game.viewport, tile.x, tile.y, &px, &py);
        int ts = game.viewport.tile_size;

        // Targetable (opposing-team) tiles draw dithered so the highlight
        // stays visible under the entity's opaque sprite, drawn later in
        // render_entities. Allies fall back to solid.
        entity_t *occupant = entity_find_at(game.entities, tile);
        bool targetable = occupant != 0 && occupant->team != attacker->team;
        if (targetable) {
            graphics_draw_rectangle_dithered(fb, fb_width, px, py, ts, ts, COLOR_ATTACK_RANGE_TINT);
        } else {
            graphics_draw_rectangle(fb, fb_width, px, py, ts, ts, COLOR_ATTACK_RANGE_TINT);
        }
    }

    // Drawn after attack_range_tiles so the blast preview layers on top of
    // it (pathing_ranges_t's blast_preview_tiles is independent of, and
    // coexists with, attack_range_tiles -- see F1-05).
    for (SLICE_FOREACH(game.pathing.blast_preview_tiles, tile_s)) {
        position_t tile = SLICE_DEREF(tile_s);
        int px, py;
        grid_to_screen(game.viewport, tile.x, tile.y, &px, &py);
        int ts = game.viewport.tile_size;
        graphics_draw_rectangle_dithered(fb, fb_width, px, py, ts, ts, COLOR_BLAST_PREVIEW_TINT);
    }

    if (game.hover_valid) {
        int px, py;
        grid_to_screen(game.viewport, game.hover.x, game.hover.y, &px, &py);
        int ts = game.viewport.tile_size;
        render_draw_outline(fb, fb_width, (rect_t){px, py, ts, ts}, COLOR_WHITE);
    }
}

PRIVATE void render_hp_bar(slice_rgba_t fb, int fb_width, int px, int py, int ts, entity_t *entity) {
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
    // With max_hp > 0 and 0 <= hp <= max_hp, fg_width is always clamped to
    // [0, bar_width] by construction. Assert the invariant instead of
    // leaving dead clamp branches behind.
    assert_debug(fg_width >= 0);
    assert_debug(fg_width <= bar_width);
    graphics_draw_rectangle(fb, fb_width, bar_x, bar_y, fg_width, bar_height, COLOR_HP_FG);
}

PRIVATE void render_entities(slice_rgba_t fb, int fb_width, game_state_t game) {
    entity_t *active = turn_active_entity(game.turn);

    for ( SLICE_FOREACH(game.entities, entity_s) ) {
        entity_t *entity = &SLICE_DEREF(entity_s);
        if (!entity->alive) {
            continue;
        }

        int px, py;
        grid_to_screen(game.viewport, entity->position.x, entity->position.y, &px, &py);
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

        if (entity == active) {
            // Always visible, regardless of mode. Distinct from the outline
            // below so the two never look identical when both apply.
            graphics_draw_rectangle(fb, fb_width, px + TURN_INDICATOR_INSET, py + TURN_INDICATOR_INSET, TURN_INDICATOR_SIZE, TURN_INDICATOR_SIZE, COLOR_TURN_INDICATOR);
        }

        if (game.mode != GAME_MODE_NONE && entity == active) {
            render_draw_outline(fb, fb_width, (rect_t){px, py, ts, ts}, COLOR_WHITE);
        }
    }
}

PRIVATE void render_timeline(slice_rgba_t fb, int fb_width, game_state_t game) {
    rect_t area = game.viewport.timeline_rect;
    int square = area.height;
    int gap = 2;
    entity_t *active = turn_active_entity(game.turn);

    for (SLICE_FOREACH(game.turn.order, entity_ptr_s)) {
        entity_t *entity = SLICE_DEREF(entity_ptr_s);
        int i = typesize(game.turn.order.begin, entity_ptr_s.begin);
        int x = area.x + i * (square + gap);

        rgba_t color = entity->team == ENTITY_TEAM_PLAYER ? COLOR_PLAYER : COLOR_ENEMY;
        graphics_draw_rectangle(fb, fb_width, x, area.y, square, square, color);

        if (entity == active) {
            render_draw_outline(fb, fb_width, (rect_t){x, area.y, square, square}, COLOR_WHITE);
        }
    }
}

PRIVATE void render_hud(slice_rgba_t fb, int fb_width, game_state_t game) {
    rect_t hud = game.viewport.hud_rect;
    graphics_draw_rectangle(fb, fb_width, hud.x, hud.y, hud.width, hud.height, COLOR_HUD_BG);

    render_timeline(fb, fb_width, game);

    entity_t *active = turn_active_entity(game.turn);
    rect_t button = game.viewport.end_turn_button;
    rgba_t button_color = (active->team == ENTITY_TEAM_PLAYER) ? COLOR_END_TURN_ACTIVE : COLOR_END_TURN_INACTIVE;
    graphics_draw_rectangle(fb, fb_width, button.x, button.y, button.width, button.height, button_color);

    rect_t attack_button = game.viewport.attack_button;
    rgba_t attack_button_color;
    if (active->team != ENTITY_TEAM_PLAYER || game.mode == GAME_MODE_NONE) {
        attack_button_color = COLOR_ATTACK_BUTTON_INACTIVE;
    } else if (game.mode == GAME_MODE_ATTACK) {
        attack_button_color = COLOR_ATTACK_BUTTON_ON;
    } else {
        attack_button_color = COLOR_ATTACK_BUTTON_AVAILABLE;
    }
    graphics_draw_rectangle(fb, fb_width, attack_button.x, attack_button.y, attack_button.width, attack_button.height, attack_button_color);

    // A single-skill entity has nothing to switch between, so no row is drawn.
    // layout_visible_skill_button_count keeps this in sync with game_on_input_event.
    int button_count = layout_visible_skill_button_count(active->team == ENTITY_TEAM_PLAYER, game.mode != GAME_MODE_NONE, entity_skill_count(active));
    for (int i = 0; i < button_count; i++) {
        rect_t skill_button = SLICE_AT(viewport_skill_buttons(&game.viewport), i);
        rgba_t skill_button_color = (i == game.selected_skill) ? COLOR_SKILL_BUTTON_SELECTED : COLOR_SKILL_BUTTON_AVAILABLE;
        graphics_draw_rectangle(fb, fb_width, skill_button.x, skill_button.y, skill_button.width, skill_button.height, skill_button_color);
    }

    if (game.mode == GAME_MODE_NONE) {
        return;
    }
    entity_t *selected = active;

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

PUBLIC void render_frame(slice_rgba_t framebuffer, int fb_width, game_state_t game) {
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
