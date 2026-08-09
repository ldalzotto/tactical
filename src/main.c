#include <stdint.h>

#include "clock.h"
#include "graphics.h"
#include "memory.h"

#define FB_WIDTH 320
#define FB_HEIGHT 240

extern unsigned char __heap_base;

typedef uint32_t window_handle_t;

__attribute__((import_module("env"), import_name("create_window")))
extern window_handle_t create_window(int width, int height);

__attribute__((import_module("env"), import_name("present_window")))
extern void present_window(window_handle_t window, uint8_t *fb_begin, uint8_t *fb_end);

__attribute__((import_module("env"), import_name("debug_log")))
extern void debug_log(void *begin, void *end);

typedef struct {
    linear_allocator_t allocator;
    slice_t framebuffer_align;
    slice_rgba_t framebuffer;
    uint32_t now_ms;
    uint32_t last_frame_ms;
    window_handle_t window;
} app_state_t;

static void render(app_state_t *state, uint32_t now_ms) {
    int shift = (int)(now_ms / 20);
    rgba_t color = {
        .r = (uint8_t)shift,
        .g = (uint8_t)(255 - shift),
        .b = (uint8_t)(shift / 2),
        .a = 255,
    };
    graphics_draw_rectangle(state->framebuffer, FB_WIDTH, 0, 0, FB_WIDTH, FB_HEIGHT, color);
}

__attribute__((export_name("init")))
app_state_t *init(uint32_t memory_size, uint32_t now_ms) {
    slice_t memory; memory.begin = &__heap_base;
    memory.end = byteoffset(memory.begin, memory_size);
    linear_allocator_t bootstrap = linear_allocator_init(memory);

    slice_t state_slice = linear_allocator_push(&bootstrap, sizeof(app_state_t));
    app_state_t *state = (app_state_t *)state_slice.begin;

    state->allocator = bootstrap;
    state->now_ms = now_ms;
    state->last_frame_ms = now_ms;

    state->framebuffer_align = linear_allocator_push_alignment(&state->allocator, _Alignof(uint32_t));
    slice_t fb_slice = linear_allocator_push(&state->allocator, (size_t)FB_WIDTH * FB_HEIGHT * sizeof(rgba_t));
    state->framebuffer.slice = fb_slice;

    state->window = create_window(FB_WIDTH, FB_HEIGHT);

    slice_t message = STR("Application initialized");
    debug_log(message.begin, message.end);

    return state;
}

__attribute__((export_name("deinit")))
void deinit(app_state_t *state) {
    linear_allocator_pop(&state->allocator, state->framebuffer.slice);
    linear_allocator_pop(&state->allocator, state->framebuffer_align);
    linear_allocator_pop(&state->allocator, (slice_t){state, typeoffset(state, 1)});
}

__attribute__((export_name("onNextFrame")))
uint32_t onNextFrame(app_state_t *state, uint32_t now_ms) {
    const uint32_t interval_ms = 16; // 60 FPS
    uint32_t wait_ms = clock_time_to_wait(now_ms, state->last_frame_ms, interval_ms);
    if (wait_ms != 0) {
        return wait_ms;
    }

    state->last_frame_ms = now_ms;
    render(state, now_ms);
    present_window(state->window, (uint8_t *)state->framebuffer.begin, (uint8_t *)state->framebuffer.end);
    return 0;
}
