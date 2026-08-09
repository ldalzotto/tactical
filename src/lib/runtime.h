#pragma once

#include "./memory.h"
#include <stdint.h>

typedef uint32_t window_handle_t;

typedef enum {
    INPUT_EVENT_MOUSE_MOVE = 0,
    INPUT_EVENT_MOUSE_CLICK = 1,
} input_event_type_t;

typedef struct {
    int32_t type;
    int32_t x;
    int32_t y;
} input_event_t;

SLICE_DEFINE(input_event_t);

void* heap_base();
window_handle_t create_window(int32_t width, int32_t height);
void present_window(window_handle_t window,  slice_t fb);
void debug_log(slice_t str);
void *poll_input_events(window_handle_t window, void *begin);
