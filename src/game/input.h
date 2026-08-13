#pragma once

#include "../lib/linkage.h"

#include "../lib/memory.h"
#include "../lib/runtime.h"

PUBLIC slice_input_event_t input_poll(linear_allocator_t* allocator, window_handle_t window);

#ifdef APP_UNITY_BUILD
#include "input.c"
#endif
