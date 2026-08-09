#pragma once

#include "game.h"

// Populates an already-initialized (via game_init), still-empty game_state_t
// with the shipped game's map+units. Caller is responsible for calling
// game_init first (with grid_width=16, grid_height=10) -- this function does
// not call game_init itself.
void scenario_setup_default(game_state_t *game);
