#include "./runtime.h"
#include "lib/linkage.h"
#include "lib/memory.h"
#include <stdint.h>

extern unsigned char __heap_base;

PUBLIC void* heap_base() {
    return &__heap_base;
}

__attribute__((import_module("env"), import_name("create_window")))
extern window_handle_t __create_window(int32_t width, int32_t height);

__attribute__((import_module("env"), import_name("present_window")))
extern void __present_window(window_handle_t window, void *fb_begin, void *fb_end);

__attribute__((import_module("env"), import_name("debug_write")))
extern void __debug_write(void *begin, void *end);

__attribute__((import_module("env"), import_name("debug_flush_line")))
extern void __debug_flush_line(void);

__attribute__((import_module("env"), import_name("poll_input_events")))
extern void *__poll_input_events(window_handle_t window, void *begin);

PUBLIC window_handle_t create_window(int32_t width, int32_t height) {
    return __create_window(width, height);
}

PUBLIC void present_window(window_handle_t window,  slice_t fb) {
    __present_window(window, fb.begin, fb.end);
}

// Writes a fragment of a debug line without terminating it -- the JS side
// buffers fragments and only emits the line (console.log) on the next
// debug_flush_line call. Lets fmt.h stream formatted output piece by piece
// (e.g. one JSON field at a time) with no C-side buffer of its own.
PUBLIC void debug_write(slice_t str) {
    __debug_write(str.begin, str.end);
}

PUBLIC void debug_flush_line(void) {
    __debug_flush_line();
}

PUBLIC void debug_log(slice_t str) {
    debug_write(str);
    debug_flush_line();
}

PUBLIC void *poll_input_events(window_handle_t window, void *begin) {
    return __poll_input_events(window, begin);
}
