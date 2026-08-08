#pragma once

#include <stdint.h>

// Fixed 60fps frame pacer using integer-only ms math. All time values are
// page-relative milliseconds (e.g. Math.floor(performance.now()) from JS);
// only deltas are meaningful. Caller owns next_frame_ms/carry_ms storage.

void clock_init(uint32_t now_ms, uint32_t *next_frame_ms, uint32_t *carry_ms);

// Returns 0 and advances *next_frame_ms/*carry_ms if a frame is due at
// now_ms. Otherwise returns the number of ms to wait before calling again.
uint32_t clock_time_to_wait(uint32_t now_ms, uint32_t *next_frame_ms, uint32_t *carry_ms);
