#pragma once

#include "../lib/linkage.h"

#include <stdbool.h>

#include "ui.h"

// Max skill buttons the HUD draws/hit-tests. entity_t.skills itself has no
// cap; callers clamp to this via entity_skill_count().
#define VIEWPORT_MAX_SKILL_BUTTONS 2

typedef struct {
    int origin_x, origin_y;      // top-left of grid viewport, screen space
    int tile_size;               // derived, not hardcoded
    int grid_width, grid_height; // tile counts (for bounds checks)
    rect_t hud_rect;
    rect_t end_turn_button;
    rect_t attack_button;
    // Backing storage; viewport_t is copied by value so a slice can't live
    // here directly -- use viewport_skill_buttons() to get one.
    rect_t skill_buttons[VIEWPORT_MAX_SKILL_BUTTONS];
    rect_t timeline_rect;
} viewport_t;

PUBLIC viewport_t layout_compute(int fb_width, int fb_height, int grid_width, int grid_height, int hud_height);
PUBLIC bool screen_to_grid(viewport_t v, int screen_x, int screen_y, int *out_tx, int *out_ty);
PUBLIC void grid_to_screen(viewport_t v, int tx, int ty, int *out_px, int *out_py);
PUBLIC slice_rect_t viewport_skill_buttons(viewport_t *v);
// Skill button count shared by render_hud (draw) and game_on_input_event (hit-test).
PUBLIC int layout_visible_skill_button_count(bool player_active, bool mode_active, int skill_count);

#ifdef APP_UNITY_BUILD
#include "layout.c"
#endif
