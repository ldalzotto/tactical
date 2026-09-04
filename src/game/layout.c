#include "layout.h"
#include "game/ui.h"
#include "lib/linkage.h"

PRIVATE int layout_min(int a, int b) {
    return a < b ? a : b;
}

PUBLIC viewport_t layout_compute(int fb_width, int fb_height, int grid_width, int grid_height, int hud_height) {
    int tile_size = layout_min(fb_width / grid_width, (fb_height - hud_height) / grid_height);

    rect_t hud_rect = {
        .x = 0,
        .y = grid_height * tile_size,
        .width = fb_width,
        .height = fb_height - grid_height * tile_size,
    };

    // HUD chrome scales with hud_rect.height so it stays proportional across framebuffer sizes.
    int hud_padding = hud_rect.height / 4;
    int button_width = hud_rect.height * 3 / 2;

    rect_t end_turn_button = {
        .x = hud_rect.x + hud_rect.width - button_width - hud_padding,
        .y = hud_rect.y + hud_padding,
        .width = button_width,
        .height = hud_rect.height - 2 * hud_padding,
    };

    rect_t attack_button = {
        .x = end_turn_button.x - button_width - hud_padding,
        .y = hud_rect.y + hud_padding,
        .width = button_width,
        .height = hud_rect.height - 2 * hud_padding,
    };

    // Reserves SKILL_BAR_WIDTH_BUDGET_SLOTS' worth of width for the skill
    // row regardless of how many buttons end up drawn there -- see
    // layout_skill_button_rect, which shrinks button width to stay inside
    // this same budget instead of growing the row.
    rect_t timeline_rect = {
        .x = hud_rect.x + hud_padding,
        .y = hud_rect.y + hud_padding / 10,
        .width = hud_rect.width - end_turn_button.width - attack_button.width
            - SKILL_BAR_WIDTH_BUDGET_SLOTS * (button_width + hud_padding) - 3 * hud_padding,
        .height = hud_rect.height / 8,
    };

    viewport_t viewport = {
        .origin_x = 0,
        .origin_y = 0,
        .tile_size = tile_size,
        .grid_width = grid_width,
        .grid_height = grid_height,
        .hud_rect = hud_rect,
        .end_turn_button = end_turn_button,
        .attack_button = attack_button,
        .timeline_rect = timeline_rect,
    };

    return viewport;
}

PRIVATE int layout_max(int a, int b) {
    return a > b ? a : b;
}

PUBLIC rect_t layout_skill_button_rect(viewport_t v, int index, int visible_count) {
    int hud_padding = v.hud_rect.height / 4;
    int reference_button_width = v.hud_rect.height * 3 / 2;

    // Total pixel budget the row has to work with, however many buttons
    // are actually visible -- see SKILL_BAR_WIDTH_BUDGET_SLOTS.
    int skill_bar_width = SKILL_BAR_WIDTH_BUDGET_SLOTS * (reference_button_width + hud_padding) - hud_padding;
    int button_width = layout_max(SKILL_BUTTON_MIN_WIDTH, (skill_bar_width - (visible_count - 1) * hud_padding) / visible_count);

    return (rect_t){
        .x = v.attack_button.x - (index + 1) * (button_width + hud_padding),
        .y = v.hud_rect.y + hud_padding,
        .width = button_width,
        .height = v.hud_rect.height - 2 * hud_padding,
    };
}

PUBLIC bool screen_to_grid(viewport_t v, int screen_x, int screen_y, int *out_tx, int *out_ty) {
    int viewport_width = v.grid_width * v.tile_size;
    int viewport_height = v.grid_height * v.tile_size;
    rect_t viewport_rect = (rect_t){v.origin_x, v.origin_y, viewport_width, viewport_height};
    if (!point_in_rect(viewport_rect, screen_x, screen_y)) {
        return false;
    }

    *out_tx = (screen_x - v.origin_x) / v.tile_size;
    *out_ty = (screen_y - v.origin_y) / v.tile_size;
    return true;
}

PUBLIC void grid_to_screen(viewport_t v, int tx, int ty, int *out_px, int *out_py) {
    *out_px = v.origin_x + tx * v.tile_size;
    *out_py = v.origin_y + ty * v.tile_size;
}

PUBLIC int layout_visible_skill_button_count(bool player_active, bool mode_active, int skill_count) {
    if (!player_active || !mode_active || skill_count <= 1) {
        return 0;
    }
    return skill_count;
}
