#include "layout.h"

static int layout_min(int a, int b) {
    return a < b ? a : b;
}

// TODO: Why do we have magic numbers here?
viewport_t layout_compute(int fb_width, int fb_height, int grid_width, int grid_height, int hud_height) {
    int tile_size = layout_min(fb_width / grid_width, (fb_height - hud_height) / grid_height);

    rect_t hud_rect = {
        .x = 0,
        .y = grid_height * tile_size,
        .width = fb_width,
        .height = fb_height - grid_height * tile_size,
    };

    rect_t end_turn_button = {
        .x = hud_rect.x + hud_rect.width - 70,
        .y = hud_rect.y + 10,
        .width = 60,
        .height = hud_rect.height - 20,
    };

    rect_t timeline_rect = {
        .x = hud_rect.x + 10,
        .y = hud_rect.y + 1,
        .width = hud_rect.width - end_turn_button.width - 20,
        .height = 5,
    };

    viewport_t viewport = {
        .origin_x = 0,
        .origin_y = 0,
        .tile_size = tile_size,
        .grid_width = grid_width,
        .grid_height = grid_height,
        .hud_rect = hud_rect,
        .end_turn_button = end_turn_button,
        .timeline_rect = timeline_rect,
    };
    return viewport;
}

bool screen_to_grid(viewport_t v, int screen_x, int screen_y, int *out_tx, int *out_ty) {
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

void grid_to_screen(viewport_t v, int tx, int ty, int *out_px, int *out_py) {
    *out_px = v.origin_x + tx * v.tile_size;
    *out_py = v.origin_y + ty * v.tile_size;
}
