#pragma once

#include "../lib/linkage.h"

#include <stdbool.h>

#include "ui.h"

// Horizontal budget for the skill button row, in units of "one fixed-size
// button + its gap" (same footprint attack_button/end_turn_button use).
// NOT a cap on how many skills are selectable -- entity_t.skills has no
// cap, and every skill is always reachable (see layout_visible_skill_button_count).
// Once the row holds more buttons than this budget was sized for, buttons
// shrink to fit (see layout_skill_button_rect) rather than the row growing
// or skills becoming unreachable.
#define SKILL_BAR_WIDTH_BUDGET_SLOTS 2

// Skill buttons never shrink narrower than this, however many are visible.
#define SKILL_BUTTON_MIN_WIDTH 4

typedef struct {
    int origin_x, origin_y;      // top-left of grid viewport, screen space
    int tile_size;               // derived, not hardcoded
    int grid_width, grid_height; // tile counts (for bounds checks)
    rect_t hud_rect;
    rect_t end_turn_button;
    rect_t attack_button;
    rect_t timeline_rect;
} viewport_t;

PUBLIC viewport_t layout_compute(int fb_width, int fb_height, int grid_width, int grid_height, int hud_height);
PUBLIC bool screen_to_grid(viewport_t v, int screen_x, int screen_y, int *out_tx, int *out_ty);
PUBLIC void grid_to_screen(viewport_t v, int tx, int ty, int *out_px, int *out_py);
// Rect for skill button `index`, out of `visible_count` buttons currently
// drawn (from layout_visible_skill_button_count) -- computed on demand
// rather than stored, so any skill count is representable with no backing
// array to size. Buttons shrink to fit SKILL_BAR_WIDTH_BUDGET_SLOTS' worth
// of width as visible_count grows past it.
PUBLIC rect_t layout_skill_button_rect(viewport_t v, int index, int visible_count);
// Skill button count shared by render_hud (draw) and game_on_input_event
// (hit-test). Every skill is visible/selectable -- there is no cap.
PUBLIC int layout_visible_skill_button_count(bool player_active, bool mode_active, int skill_count);

#ifdef APP_UNITY_BUILD
#include "layout.c"
#endif
