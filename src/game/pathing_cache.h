#pragma once

#include "../lib/linkage.h"

#include "../lib/memory.h"
#include "pathing.h"
#include "position.h"

// Pathing data shared by rendering and action execution, cached so it isn't
// recomputed on every frame or read.
// - walking_distances is the raw BFS dist grid reachable_tiles is derived
//   from; valid only while mode == GAME_MODE_MOVEMENT (nullified otherwise,
//   same as reachable_tiles). action_try_move queries it directly instead
//   of re-running the BFS.
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
//   walking_distances, then reachable_tiles, then attack_range_tiles, then
//   blast_preview_tiles (always topmost).
typedef struct {
    slice_t walking_distances_align;   // alignment padding before walking_distances when
                                        // non-empty; zero-length marker at the scratch cursor
                                        // when empty -- the region-as-a-whole marker, distinct
                                        // from walking_distances.align (pathing_state_t's own
                                        // internal marker, which nests immediately above this
                                        // one and stays zero-length once this one has already
                                        // absorbed the real alignment padding)
    pathing_state_t walking_distances; // raw BFS dist grid reachable_tiles is derived from;
                                        // valid only while mode == GAME_MODE_MOVEMENT
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
    slice_position_t blast_preview_tiles; // blast footprint under the current hover,
                                           // while an AoE skill is selected in attack mode; recomputed
                                           // on every hover move (see game_update_blast_preview)
} pathing_cache_t;

// Pops all four caches to nothing, then pushes fresh empty markers for
// walking_distances, reachable_tiles, attack_range_tiles and
// blast_preview_tiles -- scratch collapses to the pre-selection watermark
// with all four caches nullified.
PUBLIC void pathing_cache_reset(linear_allocator_t *scratch, pathing_cache_t *cache);

// Adopts `walking_distances_align`/`walking_distances` as the new
// walking_distances region and `reachable_align`/`reachable_tiles` as the
// new reachable_tiles region (caller builds them via
// linear_allocator_push_alignment then one linear_allocator_push per tile
// while iterating), then re-pushes empty attack_range_tiles and
// blast_preview_tiles on top so they stay the topmost scratch regions.
// Requires all four caches empty (call right after pathing_cache_reset).
PUBLIC void pathing_cache_set_reachable(linear_allocator_t *scratch, pathing_cache_t *cache, slice_t walking_distances_align, pathing_state_t walking_distances, slice_t reachable_align, slice_position_t reachable_tiles);

// Adopts `attack_range_align`/`attack_range_tiles` (built the same way as
// for pathing_cache_set_reachable) as the new attack_range_tiles region,
// then re-pushes an empty blast_preview_tiles on top so it stays the
// topmost scratch region. Requires walking_distances/reachable_tiles
// already empty (call right after pathing_cache_reset).
PUBLIC void pathing_cache_set_attack_range(linear_allocator_t *scratch, pathing_cache_t *cache, slice_t attack_range_align, slice_position_t attack_range_tiles);

// Adopts `blast_preview_align`/`blast_preview_tiles` (built the same way)
// as the new blast_preview_tiles region. Unlike the two setters above,
// this does *not* require reachable_tiles/attack_range_tiles to be empty --
// blast_preview_tiles is independently toggled and always the topmost
// scratch region, so it's always safe to call. The caller must have
// already cleared the previous blast_preview_tiles region first (see
// pathing_cache_clear_blast_preview) before staging new data on top of
// scratch, since this recomputes on every hover move rather than only
// right after a pathing_cache_reset.
PUBLIC void pathing_cache_set_blast_preview(linear_allocator_t *scratch, pathing_cache_t *cache, slice_t blast_preview_align, slice_position_t blast_preview_tiles);

// Pops the current blast_preview_tiles region and re-pushes a fresh empty
// marker in its place -- cheaper than a full pathing_cache_reset when only
// the preview needs clearing (e.g. hover leaving the grid, or moving to a
// tile with no valid blast), and the mandatory first step before staging a
// new non-empty preview via pathing_cache_set_blast_preview.
PUBLIC void pathing_cache_clear_blast_preview(linear_allocator_t *scratch, pathing_cache_t *cache);

#ifdef APP_UNITY_BUILD
#include "pathing_cache.c"
#endif
