#pragma once

#include "./memory.h"
#include <stdint.h>

typedef uint32_t window_handle_t;

void* heap_base();
window_handle_t create_window(int32_t width, int32_t height);
void present_window(window_handle_t window,  slice_t fb);
void debug_log(slice_t str);
