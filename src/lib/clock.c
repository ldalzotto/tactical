#include "clock.h"
#include "lib/linkage.h"
#include <stdint.h>

PUBLIC uint32_t clock_time_to_wait(uint32_t now_ms, uint32_t last_frame_ms, uint32_t interval) {
    uint32_t to_wait_ms = now_ms - last_frame_ms;
    if (to_wait_ms > interval) {
        return 0;
    }
    return to_wait_ms - interval;
}
