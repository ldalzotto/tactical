#pragma once

#include "../lib/graphics.h"
#include "game.h"

// Draws one frame of the game into framebuffer. game is passed by value, but
// its slices/structs still point at live data (grid tiles, entity list,
// pathing scratch buffers) -- rendering the "selected unit's reachable
// tiles" overlay recomputes and mutates game.pathing in place, same as any
// other pathing_compute_distances call.
void render_frame(slice_rgba_t framebuffer, int fb_width, game_state_t game);
