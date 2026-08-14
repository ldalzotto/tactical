#pragma once

#include "../lib/linkage.h"

#include "../lib/memory.h"
#include "position.h"

// Data derived purely for rendering, cached so render_frame never recomputes
// it per frame.
// - reachable_tiles and attack_range_tiles are mutually exclusive: exactly
//   one is populated at a time (the other nullified), matching the
//   move/attack toggle -- scratch only ever holds one live tile cache.
// - stacked regions of the same scratch arena: attack_range_tiles must
//   always sit after reachable_tiles in memory.
typedef struct {
    slice_t reachable_align;           // alignment padding before reachable_tiles when non-empty;
                                        // zero-length marker at the scratch cursor when empty
    slice_position_t reachable_tiles;  // tiles the selected entity can currently reach; resliced
                                        // on each recompute; nullified while attack_mode is on
    slice_t attack_range_align;        // same marker pattern as reachable_align, but for
                                        // attack_range_tiles; always the topmost scratch region
    slice_position_t attack_range_tiles; // tiles in the selected entity's skill range; populated
                                          // only while attack_mode is on, nullified otherwise
} render_cache_t;

// Pops both range caches to nothing, then pushes fresh empty markers for
// reachable_tiles and attack_range_tiles -- scratch collapses to the
// pre-selection watermark with both caches nullified.
PUBLIC void render_cache_reset(linear_allocator_t *scratch, render_cache_t *cache);

// Adopts `reachable_align`/`reachable_tiles` as the new reachable_tiles
// region (caller builds them via linear_allocator_push_alignment then one
// linear_allocator_push per tile while iterating), then re-pushes an empty
// attack_range_tiles on top so it stays the topmost scratch region.
// Requires both caches empty (call right after render_cache_reset).
PUBLIC void render_cache_write_reachable(linear_allocator_t *scratch, render_cache_t *cache, slice_t reachable_align, slice_position_t reachable_tiles);

// Adopts `attack_range_align`/`attack_range_tiles` (built the same way as
// for render_cache_write_reachable) as the new attack_range_tiles region.
// Already the topmost scratch region, so nothing is pushed after it.
// Requires reachable_tiles already empty (call right after render_cache_reset).
PUBLIC void render_cache_write_attack_range(render_cache_t *cache, slice_t attack_range_align, slice_position_t attack_range_tiles);

#ifdef APP_UNITY_BUILD
#include "render_cache.c"
#endif
