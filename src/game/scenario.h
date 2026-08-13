#pragma once

#include "../lib/linkage.h"

#include "game.h"

PUBLIC game_state_t scenario_setup_default(linear_allocator_t* allocator, int grid_width, int grid_height, int fb_width, int fb_height, int hud_height);

#ifdef APP_UNITY_BUILD
#include "scenario.c"
#endif
