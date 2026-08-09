#include "input.h"

input_state_t input_init(linear_allocator_t *allocator, window_handle_t window) {
    slice_input_event_t buffer;
    buffer = LINEAR_ALLOCATOR_PUSH(allocator, buffer, INPUT_EVENT_BUFFER_CAPACITY);

    input_state_t state = { .window = window, .buffer = buffer };
    return state;
}

void input_deinit(linear_allocator_t *allocator, input_state_t state) {
    LINEAR_ALLOCATOR_POP(allocator, state.buffer);
}

slice_input_event_t input_poll(input_state_t state) {
    void *end = poll_input_events(state.window, state.buffer.begin, state.buffer.end);

    slice_input_event_t polled = { .slice = { state.buffer.begin, end } };
    return polled;
}
