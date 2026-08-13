#pragma once

#include "../lib/linkage.h"

#include "../lib/graphics.h"
#include "game.h"

// Draws one frame of the game into framebuffer. game is passed by value, but
// its slices/structs still point at live data (grid tiles, entity list) --
// the "selected unit's reachable tiles" overlay just reads game.render.reachable_tiles,
// a cache game.c keeps up to date as selection/position/mp change.
PUBLIC void render_frame(slice_rgba_t framebuffer, int fb_width, game_state_t game);

#ifdef APP_UNITY_BUILD
#include "render.c"
#endif
