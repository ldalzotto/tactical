#pragma once

#include <stdbool.h>

#include "ui.h"

typedef struct {
    int origin_x, origin_y;      // top-left of grid viewport, screen space
    int tile_size;               // derived, not hardcoded
    int grid_width, grid_height; // tile counts (for bounds checks)
    rect_t hud_rect;
    rect_t end_turn_button;
    rect_t timeline_rect;
} viewport_t;

viewport_t layout_compute(int fb_width, int fb_height, int grid_width, int grid_height, int hud_height);
bool screen_to_grid(viewport_t v, int screen_x, int screen_y, int *out_tx, int *out_ty);
void grid_to_screen(viewport_t v, int tx, int ty, int *out_px, int *out_py);
