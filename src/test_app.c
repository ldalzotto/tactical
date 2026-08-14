#include "test_app.h"
#include "lib/assert.h"
#include "lib/memory.h"
#include "main.h"

PRIVATE void test_app_init_deinit_and_frames(linear_allocator_t *allocator) {
    // main.c's init() bootstraps its own allocator from heap_base(), so leave
    // the test allocator untouched and only use it to recover the wasm memory
    // budget. deinit() pops the app back to the same watermark, keeping the
    // test runner's final linear_allocator_deinit happy.
    uint32_t memory_size = (uint32_t)(allocator->data.end - allocator->data.begin);
    app_state_t *state = init(memory_size, 1000);
    assert_test(state != 0);

    // Drive the event-dispatch loop with a non-empty batch (the JS test
    // runner polls zero events, which would leave the loop body/increment
    // uncovered). (0,0) is an empty grid tile in the default scenario, so
    // the click is a harmless no-op.
    input_event_t click = { .type = INPUT_EVENT_MOUSE_CLICK, .x = 0, .y = 0 };
    slice_input_event_t events = { .begin = &click, .end = &click + 1 };
    app_dispatch_input_events(&state->game, &state->allocator, events);

    // 15ms elapsed since last_frame_ms (1000): below the 16ms interval, so
    // onNextFrame asks the host to wait instead of rendering.
    uint32_t wait_ms = onNextFrame(state, 1000 + 15);
    assert_test(wait_ms != 0);

    // Exactly 16ms: a frame is due. This runs input_poll, the (empty) event
    // loop, render_frame, and present_window.
    wait_ms = onNextFrame(state, 1000 + 16);
    assert_test(wait_ms == 0);

    // 20ms after the last rendered frame: also above the interval.
    wait_ms = onNextFrame(state, 1000 + 16 + 20);
    assert_test(wait_ms == 0);

    deinit(state);
}

const test_case_t g_app_tests[] = {
    { TEST_NAME("app_init_deinit_and_frames"), test_app_init_deinit_and_frames },
};

const uint32_t g_app_tests_count = sizeof(g_app_tests) / sizeof(g_app_tests[0]);
