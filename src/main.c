#include <stdint.h>

#include "memory.h"

#define FB_WIDTH 320
#define FB_HEIGHT 240

extern unsigned char __heap_base;

__attribute__((import_module("env"), import_name("create_window")))
extern void create_window(int width, int height);

typedef struct {
    linear_allocator_t allocator;
    uint8_t *framebuffer;
    float elapsed_ms;
} app_state_t;

__attribute__((export_name("init")))
app_state_t *init(uint32_t memory_size) {
    slice_t memory = { &__heap_base, (void *)(uintptr_t)memory_size };
    linear_allocator_t bootstrap = linear_allocator_init(memory);

    slice_t state_slice = linear_allocator_push(&bootstrap, sizeof(app_state_t));
    app_state_t *state = (app_state_t *)state_slice.begin;

    state->allocator = bootstrap;
    state->elapsed_ms = 0.0f;

    linear_allocator_push_alignment(&state->allocator, _Alignof(uint32_t));
    slice_t fb_slice = linear_allocator_push(&state->allocator, (size_t)FB_WIDTH * FB_HEIGHT * 4);
    state->framebuffer = (uint8_t *)fb_slice.begin;

    create_window(FB_WIDTH, FB_HEIGHT);

    return state;
}

__attribute__((export_name("get_framebuffer")))
uint8_t *get_framebuffer(app_state_t *state) {
    return state->framebuffer;
}

__attribute__((export_name("frame")))
void frame(app_state_t *state, float dt_ms) {
    state->elapsed_ms += dt_ms;
    int shift = (int)(state->elapsed_ms * 0.05f);

    for (int y = 0; y < FB_HEIGHT; y++) {
        for (int x = 0; x < FB_WIDTH; x++) {
            int idx = (y * FB_WIDTH + x) * 4;
            state->framebuffer[idx + 0] = (uint8_t)(x + shift);
            state->framebuffer[idx + 1] = (uint8_t)(y - shift);
            state->framebuffer[idx + 2] = (uint8_t)((x + y) / 2 + shift);
            state->framebuffer[idx + 3] = 255;
        }
    }
}
