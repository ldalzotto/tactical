#pragma once

#include "../lib/linkage.h"

#include "../lib/graphics.h"
#include "game.h"

// Draws one frame into framebuffer. game is by value, but its slices still
// point at live data; the reachable-tiles overlay just reads
// game.pathing.walking_distances (distance >= 1 = reachable), a cache
// game.c keeps up to date.
PUBLIC void render_frame(slice_rgba_t framebuffer, int fb_width, game_state_t game);

#ifdef APP_UNITY_BUILD
#include "render.c"
#endif
