#pragma once

#include "../lib/graphics.h"
#include "game.h"

// Draws one frame of the game into framebuffer. game is passed by value, but
// its slices/structs still point at live data (grid tiles, entity list) --
// rendering the "selected unit's reachable tiles" overlay computes a
// pathing_state_t from allocator on the fly and deinits it before returning.
void render_frame(slice_rgba_t framebuffer, int fb_width, game_state_t game, linear_allocator_t *allocator);
