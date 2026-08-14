#include "test_render.h"
#include "lib/assert.h"
#include "game/entity.h"
#include "game/game.h"
#include "game/render.h"
#include "game/skill.h"
#include "game/turn.h"
#include "test_game_helpers.h"

// render.c's turn-indicator color/inset/size are PRIVATE (file-local in every
// build mode, see linkage.h), so these tests treat the marker as a black box:
// scan for its known RGBA rather than importing render.c's constants. Keep
// this literal in sync with render.c's COLOR_TURN_INDICATOR if that ever
// changes.
static const rgba_t TEST_COLOR_TURN_INDICATOR = { 250, 210, 40, 255 };
// The pre-existing mode-gated selection outline's color (render.c's
// COLOR_WHITE) -- used to confirm the two indicators coexist without one
// overwriting the other, not just that the new one appears in isolation.
static const rgba_t TEST_COLOR_SELECTION_OUTLINE = { 255, 255, 255, 255 };

static bool test_rgba_equals(rgba_t a, rgba_t b) {
    // Bitwise & instead of &&: each channel comparison is a plain value
    // here, so this helper doesn't introduce short-circuit coverage branches
    // in every caller.
    return (a.r == b.r) & (a.g == b.g) & (a.b == b.b) & (a.a == b.a);
}

static bool test_tile_contains_color(slice_rgba_t fb, int fb_width, viewport_t viewport, position_t tile, rgba_t color) {
    int px, py;
    grid_to_screen(viewport, tile.x, tile.y, &px, &py);
    int ts = viewport.tile_size;

    for (int y = 0; y < ts; y++) {
        for (int x = 0; x < ts; x++) {
            rgba_t pixel = SLICE_AT(fb, (py + y) * fb_width + (px + x));
            if (test_rgba_equals(pixel, color)) {
                return true;
            }
        }
    }
    return false;
}

static bool test_tile_fully_color(slice_rgba_t fb, int fb_width, viewport_t viewport, position_t tile, rgba_t color) {
    int px, py;
    grid_to_screen(viewport, tile.x, tile.y, &px, &py);
    int ts = viewport.tile_size;

    for (int y = 0; y < ts; y++) {
        for (int x = 0; x < ts; x++) {
            rgba_t pixel = SLICE_AT(fb, (py + y) * fb_width + (px + x));
            if (!test_rgba_equals(pixel, color)) {
                return false;
            }
        }
    }
    return true;
}

// Framebuffer allocation, mirroring main.c's init(): push alignment padding,
// then the pixel buffer. *out_align must be popped (after popping the
// returned slice) to fully unwind the allocator -- see callers.
static slice_rgba_t test_render_alloc_framebuffer(linear_allocator_t *allocator, int fb_width, int fb_height, slice_t *out_align) {
    *out_align = linear_allocator_push_alignment(allocator, _Alignof(rgba_t));
    slice_rgba_t fb;
    fb = LINEAR_ALLOCATOR_PUSH(allocator, fb, (size_t)(fb_width * fb_height));
    return fb;
}

PRIVATE void test_render_turn_indicator_visible_before_and_after_selection(linear_allocator_t *allocator) {
    // Framebuffer is pushed before the game state (and popped after
    // game_deinit below) to respect the allocator's LIFO push/pop discipline.
    slice_t fb_align;
    slice_rgba_t fb = test_render_alloc_framebuffer(allocator, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, &fb_align);

    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t *p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t *enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 3}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *enemy_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    enemy->skills = (slice_skill_t){ .begin = enemy_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, enemy);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    // GAME_MODE_NONE: no selection made yet, but it's still p's turn.
    render_frame(fb, GAME_TEST_FB_WIDTH, game);
    assert_test(test_tile_contains_color(fb, GAME_TEST_FB_WIDTH, game.viewport, p->position, TEST_COLOR_TURN_INDICATOR));
    // Not active: no indicator on the enemy's tile.
    assert_test(!test_tile_contains_color(fb, GAME_TEST_FB_WIDTH, game.viewport, enemy->position, TEST_COLOR_TURN_INDICATOR));

    // Selecting the active entity enters GAME_MODE_MOVEMENT -- indicator
    // should still be present (it doesn't depend on mode), AND coexist with
    // the pre-existing mode-gated selection outline (not overwritten by it,
    // nor overwriting it) -- both colors must be found on the same tile.
    test_click_tile(&game, allocator, p->position);
    render_frame(fb, GAME_TEST_FB_WIDTH, game);
    assert_test(test_tile_contains_color(fb, GAME_TEST_FB_WIDTH, game.viewport, p->position, TEST_COLOR_TURN_INDICATOR));
    assert_test(test_tile_contains_color(fb, GAME_TEST_FB_WIDTH, game.viewport, p->position, TEST_COLOR_SELECTION_OUTLINE));

    game_deinit(allocator, game);
    LINEAR_ALLOCATOR_POP(allocator, fb);
    linear_allocator_pop(allocator, fb_align);
}

PRIVATE void test_render_turn_indicator_follows_active_entity_across_turns(linear_allocator_t *allocator) {
    slice_t fb_align;
    slice_rgba_t fb = test_render_alloc_framebuffer(allocator, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, &fb_align);

    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t *p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t *enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 3}, 10, 2, 0);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *enemy_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    enemy->skills = (slice_skill_t){ .begin = enemy_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, enemy);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    // enemy has 0 mp and is far from p, so ai_run_ennemy_turn is a no-op and
    // the turn comes right back around to p -- turn_active_entity(game.turn)
    // stays p, and the indicator should stay on p's tile throughout.
    test_click_end_turn(&game, allocator);
    assert_test(turn_active_entity(game.turn) == p);

    render_frame(fb, GAME_TEST_FB_WIDTH, game);
    assert_test(test_tile_contains_color(fb, GAME_TEST_FB_WIDTH, game.viewport, p->position, TEST_COLOR_TURN_INDICATOR));

    game_deinit(allocator, game);
    LINEAR_ALLOCATOR_POP(allocator, fb);
    linear_allocator_pop(allocator, fb_align);
}

PRIVATE void test_render_dithered_rectangle_checkerboards_over_background(linear_allocator_t *allocator) {
    const int w = 4, h = 4;
    slice_t fb_align;
    slice_rgba_t fb = test_render_alloc_framebuffer(allocator, w, h, &fb_align);

    rgba_t background = { 10, 10, 10, 255 };
    rgba_t fill = { 200, 50, 50, 255 };
    graphics_draw_rectangle(fb, w, 0, 0, w, h, background);
    graphics_draw_rectangle_dithered(fb, w, 0, 0, w, h, fill);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            rgba_t pixel = SLICE_AT(fb, y * w + x);
            rgba_t expected = ((x + y) % 2 == 0) ? fill : background;
            assert_test(test_rgba_equals(pixel, expected));
        }
    }

    LINEAR_ALLOCATOR_POP(allocator, fb);
    linear_allocator_pop(allocator, fb_align);
}

PRIVATE void test_render_attack_range_tile_occupied_by_enemy_is_dithered_not_solid(linear_allocator_t *allocator) {
    slice_t fb_align;
    slice_rgba_t fb = test_render_alloc_framebuffer(allocator, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, &fb_align);

    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 5, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t *p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 1, 3);
    entity_t *enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){2, 0}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_RANGED);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *enemy_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    enemy->skills = (slice_skill_t){ .begin = enemy_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, enemy);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);
    render_frame(fb, GAME_TEST_FB_WIDTH, game);

    rgba_t tint = { 230, 140, 60, 255 };
    // COLOR_TILE_WALKABLE (render.c) -- the outer-border tile color a
    // checkerboard "off" pixel falls back to once the tint isn't drawn
    // there. Grid has no obstacles, so every tile is walkable.
    rgba_t tile_walkable = { 40, 40, 40, 255 };

    // (1, 0): in range, unoccupied -- full solid tint fill (dithering only
    // applies to occupied tiles).
    assert_test(test_tile_fully_color(fb, GAME_TEST_FB_WIDTH, game.viewport, (position_t){1, 0}, tint));

    // enemy's own tile: two horizontally-adjacent pixels in the top-left
    // corner (row 0, columns 0-1 of the tile -- inside the outer 2px border
    // band, well above render_entities' square_top so the entity sprite
    // never overwrites this row) must differ: even (px+py) parity draws
    // tint, odd parity is skipped and falls through to the tile's own
    // color. A solid (non-dithered) fill would make both pixels equal tint
    // -- this is the actual proof dithering happened, not just "some tint
    // pixel exists somewhere on the tile" (which is also true of a solid
    // fill's untouched margin, so doesn't distinguish the two).
    int enemy_px, enemy_py;
    grid_to_screen(game.viewport, enemy->position.x, enemy->position.y, &enemy_px, &enemy_py);
    assert_test((enemy_px + enemy_py) % 2 == 0); // sanity: the pixel we check is the "on" one
    rgba_t corner_on = SLICE_AT(fb, enemy_py * GAME_TEST_FB_WIDTH + enemy_px);
    rgba_t corner_off = SLICE_AT(fb, enemy_py * GAME_TEST_FB_WIDTH + (enemy_px + 1));
    assert_test(test_rgba_equals(corner_on, tint));
    assert_test(test_rgba_equals(corner_off, tile_walkable));

    game_deinit(allocator, game);
    LINEAR_ALLOCATOR_POP(allocator, fb);
    linear_allocator_pop(allocator, fb_align);
}

PRIVATE void test_render_obstacle_tile_uses_obstacle_colors(linear_allocator_t *allocator) {
    slice_t fb_align;
    slice_rgba_t fb = test_render_alloc_framebuffer(allocator, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, &fb_align);

    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);
    grid_set_walkable(grid, (position_t){1, 1}, false);

    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t *p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = skills;

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    render_frame(fb, GAME_TEST_FB_WIDTH, game);

    rgba_t obstacle_outer = { 10, 10, 10, 255 };
    assert_test(test_tile_contains_color(fb, GAME_TEST_FB_WIDTH, game.viewport, (position_t){1, 1}, obstacle_outer));

    game_deinit(allocator, game);
    LINEAR_ALLOCATOR_POP(allocator, fb);
    linear_allocator_pop(allocator, fb_align);
}

PRIVATE void test_render_hover_draws_outline(linear_allocator_t *allocator) {
    slice_t fb_align;
    slice_rgba_t fb = test_render_alloc_framebuffer(allocator, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, &fb_align);

    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);

    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t *p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = skills;

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    int px, py;
    grid_to_screen(game.viewport, 2, 2, &px, &py);
    input_event_t move = { .type = INPUT_EVENT_MOUSE_MOVE, .x = px + 1, .y = py + 1 };
    game_on_input_event(&game, allocator, move);

    render_frame(fb, GAME_TEST_FB_WIDTH, game);

    rgba_t white = { 255, 255, 255, 255 };
    assert_test(test_tile_contains_color(fb, GAME_TEST_FB_WIDTH, game.viewport, (position_t){2, 2}, white));

    game_deinit(allocator, game);
    LINEAR_ALLOCATOR_POP(allocator, fb);
    linear_allocator_pop(allocator, fb_align);
}

PRIVATE void test_render_small_tile_clamps_hp_bar_and_entity_metrics(linear_allocator_t *allocator) {
    const int fb_width = 16;
    const int fb_height = 16;
    const int hud_height = 4;

    slice_t fb_align;
    slice_rgba_t fb = test_render_alloc_framebuffer(allocator, fb_width, fb_height, &fb_align);

    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 8, 4);

    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t *p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = skills;

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, fb_width, fb_height, hud_height);
    assert_test(game.viewport.tile_size == 2);

    // ts=2 drives every small-tile clamp in render_hp_bar/render_entities.
    render_frame(fb, fb_width, game);

    game_deinit(allocator, game);
    LINEAR_ALLOCATOR_POP(allocator, fb);
    linear_allocator_pop(allocator, fb_align);
}

PRIVATE void test_render_hp_bar_zero_max_hp_uses_zero_foreground(linear_allocator_t *allocator) {
    slice_t fb_align;
    slice_rgba_t fb = test_render_alloc_framebuffer(allocator, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, &fb_align);

    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);

    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t *p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = skills;

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    p->max_hp = 0;
    p->hp = 0;

    render_frame(fb, GAME_TEST_FB_WIDTH, game);

    game_deinit(allocator, game);
    LINEAR_ALLOCATOR_POP(allocator, fb);
    linear_allocator_pop(allocator, fb_align);
}

PRIVATE void test_render_skips_dead_entities(linear_allocator_t *allocator) {
    slice_t fb_align;
    slice_rgba_t fb = test_render_alloc_framebuffer(allocator, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, &fb_align);

    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);

    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t *p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t *e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *e_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e->skills = (slice_skill_t){ .begin = e_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, e);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    entity_damage(e, e->hp);

    render_frame(fb, GAME_TEST_FB_WIDTH, game);

    game_deinit(allocator, game);
    LINEAR_ALLOCATOR_POP(allocator, fb);
    linear_allocator_pop(allocator, fb_align);
}

PRIVATE void test_render_game_over_screens(linear_allocator_t *allocator) {
    slice_t fb_align;
    slice_rgba_t fb = test_render_alloc_framebuffer(allocator, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, &fb_align);

    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);

    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t *p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);
    entity_t *e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){1, 0}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *e_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e->skills = (slice_skill_t){ .begin = e_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, e);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    rgba_t win = { 0, 200, 0, 255 };
    rgba_t lose = { 200, 0, 0, 255 };

    game.game_over = GAME_OVER_WIN;
    render_frame(fb, GAME_TEST_FB_WIDTH, game);
    assert_test(test_tile_contains_color(fb, GAME_TEST_FB_WIDTH, game.viewport, (position_t){0, 0}, win));

    game.game_over = GAME_OVER_LOSE;
    render_frame(fb, GAME_TEST_FB_WIDTH, game);
    assert_test(test_tile_contains_color(fb, GAME_TEST_FB_WIDTH, game.viewport, (position_t){0, 0}, lose));

    // fb_width == 0 exercises the fb_height ternary's fallback branch.
    game.game_over = GAME_OVER_WIN;
    render_frame(fb, 0, game);

    game_deinit(allocator, game);
    LINEAR_ALLOCATOR_POP(allocator, fb);
    linear_allocator_pop(allocator, fb_align);
}

PRIVATE void test_render_skill_buttons_when_multiple_skills(linear_allocator_t *allocator) {
    slice_t fb_align;
    slice_rgba_t fb = test_render_alloc_framebuffer(allocator, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, &fb_align);

    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 1);

    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t *p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    skill_list_add(allocator, &skills, SKILL_RANGED);
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    test_click_tile(&game, allocator, p->position);
    render_frame(fb, GAME_TEST_FB_WIDTH, game);

    game_deinit(allocator, game);
    LINEAR_ALLOCATOR_POP(allocator, fb);
    linear_allocator_pop(allocator, fb_align);
}

PRIVATE void test_render_fully_color_helper_rejects_mixed_tile(linear_allocator_t *allocator) {
    slice_t fb_align;
    slice_rgba_t fb = test_render_alloc_framebuffer(allocator, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, &fb_align);

    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);

    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t *p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_list_add(allocator, &skills, SKILL_MELEE);
    p->skills = skills;

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    render_frame(fb, GAME_TEST_FB_WIDTH, game);

    rgba_t black = { 0, 0, 0, 255 };
    assert_test(!test_tile_fully_color(fb, GAME_TEST_FB_WIDTH, game.viewport, p->position, black));

    game_deinit(allocator, game);
    LINEAR_ALLOCATOR_POP(allocator, fb);
    linear_allocator_pop(allocator, fb_align);
}

PRIVATE void test_render_enemy_active_uses_inactive_hud_buttons(linear_allocator_t *allocator) {
    slice_t fb_align;
    slice_rgba_t fb = test_render_alloc_framebuffer(allocator, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, &fb_align);

    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 4);

    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t *e = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){0, 0}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_list_add(allocator, &skills, SKILL_MELEE);
    e->skills = skills;

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, e);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    render_frame(fb, GAME_TEST_FB_WIDTH, game);

    // The enemy is active, so both HUD buttons use the inactive color.
    rgba_t inactive = { 80, 80, 80, 255 };
    int bx = game.viewport.end_turn_button.x + 1;
    int by = game.viewport.end_turn_button.y + 1;
    assert_test(test_rgba_equals(SLICE_AT(fb, by * GAME_TEST_FB_WIDTH + bx), inactive));

    game_deinit(allocator, game);
    LINEAR_ALLOCATOR_POP(allocator, fb);
    linear_allocator_pop(allocator, fb_align);
}

// Mirrors the enemy-occupied attack-range test: an ally in attack range is
// the targetable == false side of render_tiles' targetable check, so it must
// be drawn solid rather than dithered.
PRIVATE void test_render_attack_range_tile_occupied_by_ally_is_solid_not_dithered(linear_allocator_t *allocator) {
    slice_t fb_align;
    slice_rgba_t fb = test_render_alloc_framebuffer(allocator, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, &fb_align);

    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 5, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t *p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 1, 3);
    entity_t *ally = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){1, 0}, 10, 1, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_RANGED);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };
    skill_t *ally_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    ally->skills = (slice_skill_t){ .begin = ally_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, ally);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    test_click_tile(&game, allocator, p->position);
    test_click_attack_toggle(&game, allocator);
    render_frame(fb, GAME_TEST_FB_WIDTH, game);

    rgba_t tint = { 230, 140, 60, 255 };

    // Same top-left corner probe as the enemy-dither test. The ally's own
    // tile is in attack range (distance 1) but is same-team, so targetable
    // is false and the tile must be a solid tint fill, not a checkerboard.
    int ally_px, ally_py;
    grid_to_screen(game.viewport, ally->position.x, ally->position.y, &ally_px, &ally_py);
    assert_test((ally_px + ally_py) % 2 == 0);
    rgba_t corner_on = SLICE_AT(fb, ally_py * GAME_TEST_FB_WIDTH + ally_px);
    rgba_t corner_off = SLICE_AT(fb, ally_py * GAME_TEST_FB_WIDTH + (ally_px + 1));
    assert_test(test_rgba_equals(corner_on, tint));
    assert_test(test_rgba_equals(corner_off, tint));

    game_deinit(allocator, game);
    LINEAR_ALLOCATOR_POP(allocator, fb);
    linear_allocator_pop(allocator, fb_align);
}

// With exactly two skills, render_hud's skill-button loop must draw without
// taking the VIEWPORT_MAX_SKILL_BUTTONS clamp (button_count > 2 is false).
PRIVATE void test_render_skill_buttons_two_skills_do_not_clamp(linear_allocator_t *allocator) {
    slice_t fb_align;
    slice_rgba_t fb = test_render_alloc_framebuffer(allocator, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, &fb_align);

    slice_t grid_padding = grid_align(allocator);
    grid_t grid = grid_init(allocator, 4, 1);
    slice_t entity_list_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t));
    slice_entity_t entities = entity_list_init(allocator);
    entity_t *p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3);

    slice_t skill_list_align = linear_allocator_push_alignment(allocator, _Alignof(skill_t));
    slice_skill_t skills = skill_list_init(allocator);
    skill_t *p_skills_begin = skills.end;
    skill_list_add(allocator, &skills, SKILL_MELEE);
    skill_list_add(allocator, &skills, SKILL_RANGED);
    p->skills = (slice_skill_t){ .begin = p_skills_begin, .end = skills.end };

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, skill_list_align, skills, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    test_click_tile(&game, allocator, p->position);
    render_frame(fb, GAME_TEST_FB_WIDTH, game);

    assert_test(game.mode == GAME_MODE_MOVEMENT);
    assert_test(game.selected_skill == 0);

    game_deinit(allocator, game);
    LINEAR_ALLOCATOR_POP(allocator, fb);
    linear_allocator_pop(allocator, fb_align);
}

const test_case_t g_render_tests[] = {
    { TEST_NAME("render_turn_indicator_visible_before_and_after_selection"), test_render_turn_indicator_visible_before_and_after_selection },
    { TEST_NAME("render_turn_indicator_follows_active_entity_across_turns"), test_render_turn_indicator_follows_active_entity_across_turns },
    { TEST_NAME("render_dithered_rectangle_checkerboards_over_background"), test_render_dithered_rectangle_checkerboards_over_background },
    { TEST_NAME("render_attack_range_tile_occupied_by_enemy_is_dithered_not_solid"), test_render_attack_range_tile_occupied_by_enemy_is_dithered_not_solid },
    { TEST_NAME("render_attack_range_tile_occupied_by_ally_is_solid_not_dithered"), test_render_attack_range_tile_occupied_by_ally_is_solid_not_dithered },
    { TEST_NAME("render_obstacle_tile_uses_obstacle_colors"), test_render_obstacle_tile_uses_obstacle_colors },
    { TEST_NAME("render_hover_draws_outline"), test_render_hover_draws_outline },
    { TEST_NAME("render_small_tile_clamps_hp_bar_and_entity_metrics"), test_render_small_tile_clamps_hp_bar_and_entity_metrics },
    { TEST_NAME("render_hp_bar_zero_max_hp_uses_zero_foreground"), test_render_hp_bar_zero_max_hp_uses_zero_foreground },
    { TEST_NAME("render_skips_dead_entities"), test_render_skips_dead_entities },
    { TEST_NAME("render_game_over_screens"), test_render_game_over_screens },
    { TEST_NAME("render_skill_buttons_when_multiple_skills"), test_render_skill_buttons_when_multiple_skills },
    { TEST_NAME("render_skill_buttons_two_skills_do_not_clamp"), test_render_skill_buttons_two_skills_do_not_clamp },
    { TEST_NAME("render_fully_color_helper_rejects_mixed_tile"), test_render_fully_color_helper_rejects_mixed_tile },
    { TEST_NAME("render_enemy_active_uses_inactive_hud_buttons"), test_render_enemy_active_uses_inactive_hud_buttons },
};

const uint32_t g_render_tests_count = sizeof(g_render_tests) / sizeof(g_render_tests[0]);
