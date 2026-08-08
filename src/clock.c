#include "clock.h"

// Target 60 frames per second using integer-only math, so the schedule
// never drifts from wall-clock time the way `now_ms % 16` would.
//
// 1000ms / 60 frames = 16ms with a remainder of 40ms every 60 frames.
// Distributing that remainder one frame at a time (Bresenham-style)
// yields the exact repeating pattern 16, 17, 17 (sum 50ms every 3
// frames). Since 60 is evenly divisible by 3, twenty repeats of that
// pattern span exactly 20 * 50 = 1000ms, with zero residual error at
// every 60-frame boundary, indefinitely.
#define FRAME_INTERVAL_MS 16
#define FRAME_REMAINDER_MS 40
#define FRAME_RATE_HZ 60

void clock_init(uint32_t now_ms, uint32_t *next_frame_ms, uint32_t *carry_ms) {
    *next_frame_ms = now_ms;
    *carry_ms = 0;
}

uint32_t clock_time_to_wait(uint32_t now_ms, uint32_t *next_frame_ms, uint32_t *carry_ms) {
    if (now_ms < *next_frame_ms) {
        return *next_frame_ms - now_ms;
    }

    uint32_t interval = FRAME_INTERVAL_MS;
    *carry_ms += FRAME_REMAINDER_MS;
    if (*carry_ms >= FRAME_RATE_HZ) {
        *carry_ms -= FRAME_RATE_HZ;
        interval += 1;
    }
    *next_frame_ms += interval;

    return 0;
}
