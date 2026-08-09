#include <stdint.h>

#include "lib/clock.h"
#include "lib/graphics.h"
#include "lib/memory.h"
#include "lib/runtime.h"
#include "game/game.h"
#include "game/input.h"
#include "game/render.h"
#include "game/scenario.h"

#define FB_WIDTH 320
#define FB_HEIGHT 240
#define GRID_WIDTH 16
#define GRID_HEIGHT 10
#define HUD_HEIGHT 40

typedef struct {
    linear_allocator_t allocator;
    slice_t framebuffer_align;
    slice_rgba_t framebuffer;
    uint32_t now_ms;
    uint32_t last_frame_ms;
    window_handle_t window;
    game_state_t game;
} app_state_t;

SLICE_DEFINE(app_state_t);

__attribute__((export_name("init")))
app_state_t *init(uint32_t memory_size, uint32_t now_ms) {
    slice_t memory; memory.begin = heap_base();
    memory.end = byteoffset(memory.begin, memory_size);
    linear_allocator_t bootstrap = linear_allocator_init(memory);

    slice_app_state_t state_slice = LINEAR_ALLOCATOR_PUSH(&bootstrap, state_slice, 1);
    app_state_t *state = state_slice.begin;

    state->allocator = bootstrap;
    state->now_ms = now_ms;
    state->last_frame_ms = now_ms;

    state->framebuffer_align = LINEAR_ALLOCATOR_PUSH_ALIGNMENT(&state->allocator, state->framebuffer);
    state->framebuffer = LINEAR_ALLOCATOR_PUSH(&state->allocator, state->framebuffer, FB_WIDTH * FB_HEIGHT);

    state->window = create_window(FB_WIDTH, FB_HEIGHT);
    state->game = game_init(&state->allocator, GRID_WIDTH, GRID_HEIGHT, FB_WIDTH, FB_HEIGHT, HUD_HEIGHT);
    scenario_setup_default(&state->allocator, &state->game);

    debug_log(STR("Application initialized"));

    return state;
}

__attribute__((export_name("deinit")))
void deinit(app_state_t *state) {
    game_deinit(&state->allocator, state->game);
    LINEAR_ALLOCATOR_POP(&state->allocator, state->framebuffer);
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

    slice_input_event_t events = input_poll(&state->allocator, state->window);
    for (input_event_t *event = events.begin; event != events.end; event++) {
        game_on_input_event(&state->game, *event);
    }
    linear_allocator_pop(&state->allocator, events.slice);

    render_frame(state->framebuffer, FB_WIDTH, state->game);
    present_window(state->window, state->framebuffer.slice);
    return 0;
}
