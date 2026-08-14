#pragma once

#include "../lib/linkage.h"

#include <stdbool.h>

#include "ui.h"

// Fixed button-array size for skill selection (ticket 006). Deliberately not
// entity.h's ENTITY_MAX_SKILLS -- layout.h/.c is pure screen geometry with
// no game-logic dependency today, and this keeps it that way. The two
// happen to match by convention (both track "how many skills a loadout
// supports"); if that ever needs to diverge, they're free to.
#define VIEWPORT_MAX_SKILL_BUTTONS 2

typedef struct {
    int origin_x, origin_y;      // top-left of grid viewport, screen space
    int tile_size;               // derived, not hardcoded
    int grid_width, grid_height; // tile counts (for bounds checks)
    rect_t hud_rect;
    rect_t end_turn_button;
    rect_t attack_button;
    rect_t skill_buttons[VIEWPORT_MAX_SKILL_BUTTONS]; // one per entity_t.skills slot
    rect_t timeline_rect;
} viewport_t;

PUBLIC viewport_t layout_compute(int fb_width, int fb_height, int grid_width, int grid_height, int hud_height);
PUBLIC bool screen_to_grid(viewport_t v, int screen_x, int screen_y, int *out_tx, int *out_ty);
PUBLIC void grid_to_screen(viewport_t v, int tx, int ty, int *out_px, int *out_py);

#ifdef APP_UNITY_BUILD
#include "layout.c"
#endif
