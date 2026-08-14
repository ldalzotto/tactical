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

static bool test_tile_contains_color(slice_rgba_t fb, int fb_width, viewport_t viewport, position_t tile, rgba_t color) {
    int px, py;
    grid_to_screen(viewport, tile.x, tile.y, &px, &py);
    int ts = viewport.tile_size;

    for (int y = 0; y < ts; y++) {
        for (int x = 0; x < ts; x++) {
            rgba_t pixel = SLICE_AT(fb, (py + y) * fb_width + (px + x));
            if (pixel.r == color.r && pixel.g == color.g && pixel.b == color.b && pixel.a == color.a) {
                return true;
            }
        }
    }
    return false;
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
    entity_t *p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3, SKILL_MELEE);
    entity_t *enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 3}, 10, 2, 3, SKILL_MELEE);

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, enemy);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

    // GAME_MODE_NONE: no selection made yet, but it's still p's turn.
    render_frame(fb, GAME_TEST_FB_WIDTH, game);
    assert_test(test_tile_contains_color(fb, GAME_TEST_FB_WIDTH, game.viewport, p->position, TEST_COLOR_TURN_INDICATOR));
    // Not active: no indicator on the enemy's tile.
    assert_test(!test_tile_contains_color(fb, GAME_TEST_FB_WIDTH, game.viewport, enemy->position, TEST_COLOR_TURN_INDICATOR));

    // Selecting the active entity enters GAME_MODE_MOVEMENT -- indicator
    // should still be present (it doesn't depend on mode).
    test_click_tile(&game, allocator, p->position);
    render_frame(fb, GAME_TEST_FB_WIDTH, game);
    assert_test(test_tile_contains_color(fb, GAME_TEST_FB_WIDTH, game.viewport, p->position, TEST_COLOR_TURN_INDICATOR));

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
    entity_t *p = entity_spawn(allocator, &entities, ENTITY_TEAM_PLAYER, (position_t){0, 0}, 10, 2, 3, SKILL_MELEE);
    entity_t *enemy = entity_spawn(allocator, &entities, ENTITY_TEAM_ENEMY, (position_t){3, 3}, 10, 2, 0, SKILL_MELEE);

    slice_t turn_order_align = linear_allocator_push_alignment(allocator, _Alignof(entity_t*));
    slice_entity_ptr_t order = turn_order_init(allocator);
    turn_order_add(allocator, &order, p);
    turn_order_add(allocator, &order, enemy);

    game_state_t game = game_init(allocator, grid_padding, grid, entity_list_align, entities, turn_order_align, order, GAME_TEST_FB_WIDTH, GAME_TEST_FB_HEIGHT, GAME_TEST_HUD_HEIGHT);

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

const test_case_t g_render_tests[] = {
    { TEST_NAME("render_turn_indicator_visible_before_and_after_selection"), test_render_turn_indicator_visible_before_and_after_selection },
    { TEST_NAME("render_turn_indicator_follows_active_entity_across_turns"), test_render_turn_indicator_follows_active_entity_across_turns },
};

const uint32_t g_render_tests_count = sizeof(g_render_tests) / sizeof(g_render_tests[0]);
