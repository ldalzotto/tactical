#pragma once

#include "linkage.h"

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

PUBLIC void* heap_base();
PUBLIC window_handle_t create_window(int32_t width, int32_t height);
PUBLIC void present_window(window_handle_t window,  slice_t fb);
PUBLIC void debug_write(slice_t str);
PUBLIC void debug_flush_line(void);
PUBLIC void *poll_input_events(window_handle_t window, void *begin);

#ifdef APP_UNITY_BUILD
#include "runtime.c"
#endif
