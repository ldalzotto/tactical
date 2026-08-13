#pragma once

#include "linkage.h"

#include <stdint.h>

// Returns 0 and advances *next_frame_ms/*carry_ms if a frame is due at
// now_ms. Otherwise returns the number of ms to wait before calling again.
PUBLIC uint32_t clock_time_to_wait(uint32_t now_ms, uint32_t last_frame_ms, uint32_t interval);

#ifdef APP_UNITY_BUILD
#include "clock.c"
#endif
