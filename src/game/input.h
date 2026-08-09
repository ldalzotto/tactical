#pragma once

#include "../lib/memory.h"
#include "../lib/runtime.h"

#define INPUT_EVENT_BUFFER_CAPACITY 64

typedef struct {
    window_handle_t window;
    slice_input_event_t buffer;
} input_state_t;

input_state_t input_init(linear_allocator_t *allocator, window_handle_t window);
void input_deinit(linear_allocator_t *allocator, input_state_t state);
slice_input_event_t input_poll(input_state_t state);
