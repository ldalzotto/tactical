#pragma once

#include <stdint.h>

#include "lib/graphics.h"
#include "lib/memory.h"
#include "lib/runtime.h"
#include "game/game.h"

typedef struct app_state_t {
    linear_allocator_t allocator;
    slice_t framebuffer_align;
    slice_rgba_t framebuffer;
    uint32_t now_ms;
    uint32_t last_frame_ms;
    window_handle_t window;
    game_state_t game;
} app_state_t;

app_state_t *app_init(uint32_t memory_size, uint32_t now_ms);
void app_deinit(app_state_t *state);
uint32_t app_on_next_frame(app_state_t *state, uint32_t now_ms);

// Dispatches a batch of already-polled input events through the game API.
// Split out of app_on_next_frame so tests can exercise the event loop with a
// non-empty slice (the wasm test runner otherwise always polls zero events).
// Returns the byte shift applied to `events` by scratch growth (0 if none);
// caller must rebase its own copy before using/popping it.
PUBLIC ptrdiff_t app_dispatch_input_events(game_state_t *game, linear_allocator_t *allocator, slice_input_event_t events);
