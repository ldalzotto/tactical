#include <stdint.h>

#include "clock.h"
#include "memory.h"

SLICE_DEFINE(uint8_t);

#define FB_WIDTH 320
#define FB_HEIGHT 240

extern unsigned char __heap_base;

__attribute__((import_module("env"), import_name("create_window")))
extern void create_window(int width, int height);

typedef struct {
    linear_allocator_t allocator;
    slice_t framebuffer_align;
    slice_uint8_t framebuffer;
    uint32_t now_ms;
    uint32_t last_frame_ms;
} app_state_t;

static void render(app_state_t *state, uint32_t now_ms) {
    int shift = (int)(now_ms / 20);

    for (int y = 0; y < FB_HEIGHT; y++) {
        for (int x = 0; x < FB_WIDTH; x++) {
            int idx = (y * FB_WIDTH + x) * 4;
            SLICE_AT(state->framebuffer, idx + 0) = (uint8_t)(x + shift);
            SLICE_AT(state->framebuffer, idx + 1) = (uint8_t)(y - shift);
            SLICE_AT(state->framebuffer, idx + 2) = (uint8_t)((x + y) / 2 + shift);
            SLICE_AT(state->framebuffer, idx + 3) = 255;
        }
    }
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
    slice_t fb_slice = linear_allocator_push(&state->allocator, (size_t)FB_WIDTH * FB_HEIGHT * 4);
    state->framebuffer.slice = fb_slice;

    create_window(FB_WIDTH, FB_HEIGHT);

    return state;
}

__attribute__((export_name("deinit")))
void deinit(app_state_t *state) {
    linear_allocator_pop(&state->allocator, state->framebuffer.slice);
    linear_allocator_pop(&state->allocator, state->framebuffer_align);
    linear_allocator_pop(&state->allocator, (slice_t){state, state + 1});
}

__attribute__((export_name("get_framebuffer")))
uint8_t *get_framebuffer(app_state_t *state) {
    return state->framebuffer.begin;
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
    return 0;
}
