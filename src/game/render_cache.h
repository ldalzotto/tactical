#pragma once

#include "../lib/linkage.h"

#include "../lib/memory.h"
#include "position.h"

// Data derived purely for rendering, cached so render_frame never recomputes
// it per frame.
// - reachable_tiles and attack_range_tiles are mutually exclusive: exactly
//   one is populated at a time (the other nullified), matching the
//   move/attack toggle -- scratch only ever holds one live tile cache
//   between the two of them.
// - blast_preview_tiles is independent of that pair: it can be populated at
//   the same time as attack_range_tiles (while attack_range_tiles is
//   populated and the hovered tile is a valid AoE impact), so the blast
//   preview layers on top of the attack-range overlay instead of replacing
//   it. It's always nullified whenever reachable_tiles is populated --
//   movement mode has no attack preview.
// - stacked regions of the same scratch arena, in this order:
//   reachable_tiles, then attack_range_tiles, then blast_preview_tiles
//   (always topmost).
typedef struct {
    slice_t reachable_align;           // alignment padding before reachable_tiles when non-empty;
                                        // zero-length marker at the scratch cursor when empty
    slice_position_t reachable_tiles;  // tiles the selected entity can currently reach; resliced
                                        // on each recompute; nullified while mode is GAME_MODE_ATTACK
    slice_t attack_range_align;        // same marker pattern as reachable_align, but for
                                        // attack_range_tiles
    slice_position_t attack_range_tiles; // tiles in the selected entity's skill range; populated
                                          // only while mode is GAME_MODE_ATTACK, nullified otherwise
    slice_t blast_preview_align;       // same marker pattern, but for blast_preview_tiles; always
                                        // the topmost scratch region
    slice_position_t blast_preview_tiles; // cover-shadowed blast footprint under the current hover,
                                           // while an AoE skill is selected in attack mode; recomputed
                                           // on every hover move (see game_update_blast_preview)
} render_cache_t;

// Pops all three range caches to nothing, then pushes fresh empty markers
// for reachable_tiles, attack_range_tiles and blast_preview_tiles --
// scratch collapses to the pre-selection watermark with all three caches
// nullified.
PUBLIC void render_cache_reset(linear_allocator_t *scratch, render_cache_t *cache);

// Adopts `reachable_align`/`reachable_tiles` as the new reachable_tiles
// region (caller builds them via linear_allocator_push_alignment then one
// linear_allocator_push per tile while iterating), then re-pushes empty
// attack_range_tiles and blast_preview_tiles on top so they stay the
// topmost scratch regions. Requires all three caches empty (call right
// after render_cache_reset).
PUBLIC void render_cache_set_reachable(linear_allocator_t *scratch, render_cache_t *cache, slice_t reachable_align, slice_position_t reachable_tiles);

// Adopts `attack_range_align`/`attack_range_tiles` (built the same way as
// for render_cache_set_reachable) as the new attack_range_tiles region,
// then re-pushes an empty blast_preview_tiles on top so it stays the
// topmost scratch region. Requires reachable_tiles already empty (call
// right after render_cache_reset).
PUBLIC void render_cache_set_attack_range(linear_allocator_t *scratch, render_cache_t *cache, slice_t attack_range_align, slice_position_t attack_range_tiles);

// Adopts `blast_preview_align`/`blast_preview_tiles` (built the same way)
// as the new blast_preview_tiles region. Unlike the two setters above,
// this does *not* require reachable_tiles/attack_range_tiles to be empty --
// blast_preview_tiles is independently toggled and always the topmost
// scratch region, so it's always safe to call. The caller must have
// already cleared the previous blast_preview_tiles region first (see
// render_cache_clear_blast_preview) before staging new data on top of
// scratch, since this recomputes on every hover move rather than only
// right after a render_cache_reset.
PUBLIC void render_cache_set_blast_preview(linear_allocator_t *scratch, render_cache_t *cache, slice_t blast_preview_align, slice_position_t blast_preview_tiles);

// Pops the current blast_preview_tiles region and re-pushes a fresh empty
// marker in its place -- cheaper than a full render_cache_reset when only
// the preview needs clearing (e.g. hover leaving the grid, or moving to a
// tile with no valid blast), and the mandatory first step before staging a
// new non-empty preview via render_cache_set_blast_preview.
PUBLIC void render_cache_clear_blast_preview(linear_allocator_t *scratch, render_cache_t *cache);

#ifdef APP_UNITY_BUILD
#include "render_cache.c"
#endif
