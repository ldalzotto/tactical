#include "input.h"
#include "lib/linkage.h"
#include "lib/memory.h"
#include "lib/runtime.h"

PUBLIC slice_input_event_t input_poll(linear_allocator_t* allocator, window_handle_t window) {
    slice_input_event_t events = LINEAR_ALLOCATOR_PUSH(allocator, events, 0);
    events.end = poll_input_events(window, events.begin);
    // We simulate the js side to have allocated some memory to the allocator
    allocator->cursor = events.end;
    return events;
}
