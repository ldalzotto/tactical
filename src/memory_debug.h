#pragma once

#include "memory.h"

typedef enum {
    MEMORY_VIOLATION_USE_AFTER_FREE,
    MEMORY_VIOLATION_OVERLAP_ON_TRACK,
    MEMORY_VIOLATION_DOUBLE_TRACK,
    MEMORY_VIOLATION_DOUBLE_UNTRACK,
} memory_violation_kind_t;

void memory_debug_track(slice_t *s);
void memory_debug_untrack(slice_t *s);
void memory_debug_check_access(slice_t s, void *addr);
