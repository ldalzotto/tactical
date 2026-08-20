#pragma once

#include "game/game.h"

// Asserts properties that must hold on game_state_t after every input event
// (see game_on_input_event). Called from test_game_helpers.h's click/move
// wrappers, so every test that drives the game through them gets this for
// free without asserting it itself.
PUBLIC void assert_game_invariants(game_state_t *game);

#ifdef APP_UNITY_BUILD
#include "test_invariants.c"
#endif
