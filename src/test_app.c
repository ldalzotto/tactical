#include "test_app.h"
#include "game/game.h"
#include "lib/assert.h"
#include "lib/linkage.h"
#include "lib/memory.h"
#include "game/layout.h"
#include "app.h"
#include "lib/runtime.h"
#include "test.h"
#include <stddef.h>
#include <stdint.h>

// Test-only host import (see web/wasm-shared.js): queues an input event for
// the next input_poll. The wasm test runner otherwise always polls zero
// events, leaving app_on_next_frame's shift!=0 rebase path uncovered.
__attribute__((import_module("env"), import_name("test_push_input_event")))
extern void test_push_input_event(window_handle_t window, int32_t type, int32_t x, int32_t y);

PRIVATE void test_app_init_deinit_and_frames(linear_allocator_t *allocator) {
    // app.c's app_init() bootstraps its own allocator from heap_base(), so leave
    // the test allocator untouched and only use it to recover the wasm memory
    // budget. app_deinit() pops the app back to the same watermark, keeping the
    // test runner's final linear_allocator_deinit happy.
    uint32_t memory_size = (uint32_t)(allocator->data.end - allocator->data.begin);
    app_state_t *state = app_init(memory_size, 1000);
    assert_test(state != 0);

    // (0,0) is an empty grid tile in the default scenario, so this click is a
    // harmless no-op that exercises the shift==0 side of the rebase guard.
    input_event_t click = { .type = INPUT_EVENT_MOUSE_CLICK, .x = 0, .y = 0 };
    slice_input_event_t events = { .begin = &click, .end = &click + 1 };
    ptrdiff_t shift = app_dispatch_input_events(&state->game, &state->allocator, events);
    assert_test(shift == 0);

    // 15ms elapsed since last_frame_ms (1000): below the 16ms interval, so
    // app_on_next_frame asks the host to wait instead of rendering.
    uint32_t wait_ms = app_on_next_frame(state, 1000 + 15);
    assert_test(wait_ms != 0);

    // Click the active entity's own tile (p1 at (1,2)): enters movement mode,
    // which grows game->scratch, exercising the shift!=0 rebase path.
    int px, py;
    grid_to_screen(state->game.viewport, 1, 2, &px, &py);
    test_push_input_event(state->window, INPUT_EVENT_MOUSE_CLICK, px + 1, py + 1);

    // Exactly 16ms: a frame is due. This runs input_poll, the event loop
    // (now with the queued click above), render_frame, and present_window.
    wait_ms = app_on_next_frame(state, 1000 + 16);
    assert_test(wait_ms == 0);
    assert_test(state->game.mode == GAME_MODE_MOVEMENT);

    // 20ms after the last rendered frame: also above the interval.
    wait_ms = app_on_next_frame(state, 1000 + 16 + 20);
    assert_test(wait_ms == 0);

    app_deinit(state);
}

const test_case_t g_app_tests[] = {
    { TEST_NAME("app_init_deinit_and_frames"), test_app_init_deinit_and_frames },
};

const uint32_t g_app_tests_count = sizeof(g_app_tests) / sizeof(g_app_tests[0]);
