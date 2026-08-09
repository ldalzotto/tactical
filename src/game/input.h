#pragma once

#include "../lib/memory.h"
#include "../lib/runtime.h"

slice_input_event_t input_poll(linear_allocator_t* allocator, window_handle_t window);
