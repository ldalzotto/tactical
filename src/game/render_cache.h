#pragma once

#include "../lib/linkage.h"

#include "../lib/memory.h"
#include "position.h"

// Data derived purely for rendering, cached so render_frame never has to
// recompute it per frame.
// reachable_tiles and attack_range_tiles are mutually exclusive: exactly one
// of the two is ever populated at a time (the other is kept nullified),
// matching the move/attack toggle -- so scratch only ever holds one live
// tile cache. The two are stacked regions of the same scratch arena:
// attack_range_tiles must always sit after reachable_tiles in memory.
typedef struct {
    slice_t reachable_align;           // alignment padding pushed into scratch right before
                                        // reachable_tiles, when it's non-empty; zero-length
                                        // marker at the current scratch cursor when it's empty
    slice_position_t reachable_tiles;  // tiles the selected entity can currently reach; length is
                                        // resliced on each recompute to reflect the live count;
                                        // nullified while attack_mode is on
    slice_t attack_range_align;        // same alignment-marker pattern as reachable_align, but for
                                        // attack_range_tiles; always the topmost region in scratch
    slice_position_t attack_range_tiles; // tiles within the selected entity's skill range, populated
                                          // only while attack_mode is on; nullified otherwise
} render_cache_t;

// Pops both range caches down to nothing, then pushes fresh empty markers
// for reachable_tiles and attack_range_tiles, collapsing scratch back to
// the pre-selection watermark with both caches nullified.
PUBLIC void render_cache_reset(linear_allocator_t *scratch, render_cache_t *cache);

// Replaces reachable_tiles with a fresh `count`-sized region and re-pushes
// an empty attack_range_tiles on top of it, so attack_range_tiles stays the
// topmost scratch region. Caller fills the `count` positions afterward via
// SLICE_AT(cache->reachable_tiles, i). Requires both caches to currently be
// empty (call right after render_cache_reset).
PUBLIC void render_cache_write_reachable(linear_allocator_t *scratch, render_cache_t *cache, int count);

// Replaces attack_range_tiles with a fresh `count`-sized region. Caller
// fills the `count` positions afterward via
// SLICE_AT(cache->attack_range_tiles, i). Requires reachable_tiles to
// already be empty (call right after render_cache_reset).
PUBLIC void render_cache_write_attack_range(linear_allocator_t *scratch, render_cache_t *cache, int count);

#ifdef APP_UNITY_BUILD
#include "render_cache.c"
#endif
