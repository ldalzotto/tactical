#include "memory_debug.h"

#ifndef NDEBUG

#include <stdbool.h>
#include <stdint.h>

#include "assert.h"

#define MEMORY_DEBUG_SLOT_BITS 8
#define MEMORY_DEBUG_MAX_TRACKED (1u << MEMORY_DEBUG_SLOT_BITS)
#define MEMORY_DEBUG_SLOT_MASK (MEMORY_DEBUG_MAX_TRACKED - 1)

typedef struct {
    slice_t slice;
    uint32_t generation;
    uint32_t alloc_backtrace_id;
    uint32_t free_backtrace_id;
    bool alive;
} tracked_slice_t;

static tracked_slice_t g_tracked[MEMORY_DEBUG_MAX_TRACKED];

__attribute__((import_module("env"), import_name("debug_capture_backtrace")))
extern uint32_t debug_capture_backtrace(void);

__attribute__((import_module("env"), import_name("debug_report_violation")))
extern void debug_report_violation(uint32_t kind, void *addr,
    uint32_t access_backtrace_id, uint32_t alloc_backtrace_id, uint32_t free_backtrace_id);

static void report(memory_violation_kind_t kind, void *addr, uint32_t alloc_backtrace_id, uint32_t free_backtrace_id) {
    uint32_t access_backtrace_id = debug_capture_backtrace();
    debug_report_violation((uint32_t)kind, addr, access_backtrace_id, alloc_backtrace_id, free_backtrace_id);
}

static bool slices_overlap(slice_t a, slice_t b) {
    return a.begin < b.end && b.begin < a.end;
}

void memory_debug_track(slice_t *s) {
    if (s->debug_id != 0) {
        report(MEMORY_VIOLATION_DOUBLE_TRACK, s->begin, 0, 0);
        return;
    }

    for (uint32_t i = 0; i < MEMORY_DEBUG_MAX_TRACKED; i++) {
        if (g_tracked[i].alive && slices_overlap(g_tracked[i].slice, *s)) {
            report(MEMORY_VIOLATION_OVERLAP_ON_TRACK, s->begin, g_tracked[i].alloc_backtrace_id, g_tracked[i].free_backtrace_id);
            return;
        }
    }

    uint32_t slot = MEMORY_DEBUG_MAX_TRACKED;
    for (uint32_t i = 0; i < MEMORY_DEBUG_MAX_TRACKED; i++) {
        if (!g_tracked[i].alive) {
            slot = i;
            break;
        }
    }
    assert(slot != MEMORY_DEBUG_MAX_TRACKED);

    tracked_slice_t *entry = &g_tracked[slot];
    entry->slice = *s;
    entry->generation++;
    entry->alloc_backtrace_id = debug_capture_backtrace();
    entry->free_backtrace_id = 0;
    entry->alive = true;

    s->debug_id = (entry->generation << MEMORY_DEBUG_SLOT_BITS) | slot;
}

void memory_debug_untrack(slice_t *s) {
    uint32_t slot = s->debug_id & MEMORY_DEBUG_SLOT_MASK;
    uint32_t generation = s->debug_id >> MEMORY_DEBUG_SLOT_BITS;

    if (s->debug_id == 0 || !g_tracked[slot].alive || g_tracked[slot].generation != generation) {
        report(MEMORY_VIOLATION_DOUBLE_UNTRACK, s->begin, 0, 0);
        return;
    }

    g_tracked[slot].free_backtrace_id = debug_capture_backtrace();
    g_tracked[slot].alive = false;
    s->debug_id = 0;
}

void memory_debug_check_access(slice_t s, void *addr) {
    if (s.debug_id == 0) {
        return;
    }

    uint32_t slot = s.debug_id & MEMORY_DEBUG_SLOT_MASK;
    uint32_t generation = s.debug_id >> MEMORY_DEBUG_SLOT_BITS;
    tracked_slice_t *entry = &g_tracked[slot];

    if (!entry->alive || entry->generation != generation) {
        report(MEMORY_VIOLATION_USE_AFTER_FREE, addr, entry->alloc_backtrace_id, entry->free_backtrace_id);
    }
}

#else

void memory_debug_track(slice_t *s) {
    (void)s;
}

void memory_debug_untrack(slice_t *s) {
    (void)s;
}

void memory_debug_check_access(slice_t s, void *addr) {
    (void)s;
    (void)addr;
}

#endif
