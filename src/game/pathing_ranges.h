#pragma once

#include "../lib/linkage.h"

#include "../lib/memory.h"
#include "grid.h"
#include "pathing.h"
#include "position.h"

// Range data for the currently selected entity: rendering reads it for
// overlays, action.c reads it to validate moves/attacks. Backed by three
// stacked regions in `scratch`, in this order: walking_distances,
// attack_range_tiles, blast_preview_tiles (topmost).
//
// - walking_distances: raw BFS dist grid, valid only in GAME_MODE_MOVEMENT.
//   action_try_move queries it to price a move, and render.c queries it
//   directly for the movement overlay (a tile is reachable iff its distance
//   is >= 1) -- there's no separate reachable-tiles tile list to keep in
//   sync with it.
// - walking_distances vs. attack_range_tiles: mutually exclusive, matching
//   the move/attack mode toggle (attack_range_tiles is nullified in
//   movement mode; walking_distances collapses to an empty marker in attack
//   mode).
// - blast_preview_tiles: independent of the pair above. Can coexist with
//   attack_range_tiles (attack mode, AoE skill, valid hover), layering the
//   blast preview on top of the range overlay. Always nullified in
//   movement mode.
typedef struct {
    slice_t walking_distances_align;   // marker: alignment padding, or empty-region marker
    pathing_state_t walking_distances; // BFS dist grid; valid only in GAME_MODE_MOVEMENT
    slice_t attack_range_align;        // marker for attack_range_tiles
    slice_position_t attack_range_tiles; // tiles in the selected skill's range, attack mode only
    slice_t blast_preview_align;       // marker for blast_preview_tiles
    slice_position_t blast_preview_tiles; // AoE footprint under the current hover
    position_t blast_preview_impact;   // tile blast_preview_tiles was computed for
    bool blast_preview_valid;          // true iff blast_preview_tiles/_impact hold real data
} pathing_ranges_t;

// Builds a fresh pathing_ranges_t backed by three empty stacked regions
// pushed onto `scratch`. Call once, right after `scratch` itself is
// initialized (see game_init).
PUBLIC pathing_ranges_t pathing_ranges_init(linear_allocator_t *scratch);

// Pops all three regions off `scratch`, in reverse of pathing_ranges_init's
// push order. Call once, right before `scratch` itself is torn down (see
// game_deinit).
PUBLIC void pathing_ranges_deinit(linear_allocator_t *scratch, pathing_ranges_t ranges);

// Collapses all three regions back to empty markers at the pre-selection watermark.
PUBLIC void pathing_ranges_reset(linear_allocator_t *scratch, pathing_ranges_t *ranges);

// Grows `scratch` in place if needed to fit `temp`'s dist array, copies it in
// as the new walking_distances, pops `temp` off `allocator`, and re-pushes
// empty attack_range_tiles/blast_preview_tiles on top. Call right after
// pathing_ranges_reset (requires all three regions empty). `temp` is rebased
// in place if growth relocates memory. Returns the shift applied (0 if it
// already fit); callers must propagate it to anything else held above
// `scratch`.
PUBLIC ptrdiff_t pathing_ranges_push_walking_distances(linear_allocator_t *allocator, linear_allocator_t *scratch, pathing_ranges_t *ranges, pathing_state_t *temp);

// Grows `scratch` in place if needed to fit `temp_tiles`, copies
// `temp_align`/`temp_tiles` into it as the new attack_range_tiles, pops the
// caller's staged copy off `allocator`, and re-pushes an empty
// blast_preview_tiles on top. Call right after pathing_ranges_reset. Returns
// the shift applied (0 if it already fit); callers must propagate it to
// anything else held above `scratch`.
PUBLIC ptrdiff_t pathing_ranges_push_attack_range(linear_allocator_t *allocator, linear_allocator_t *scratch, pathing_ranges_t *ranges, slice_t temp_align, slice_position_t temp_tiles);

// Computes the blast footprint centered on `impact` (pathing_compute_blast_tiles),
// staged on `allocator`, then grows `scratch` in place if needed to fit it
// and copies it in as the new blast_preview_tiles, popping the staged copy
// off `allocator` and setting blast_preview_valid. Safe to call any time
// (unlike the pushes above, doesn't require reachable/attack_range to be
// empty) as long as the previous preview was cleared first via
// pathing_ranges_clear_blast_preview. Returns the shift applied (0 if it
// already fit); callers must propagate it to anything else held above
// `scratch`.
PUBLIC ptrdiff_t pathing_ranges_push_blast_preview(linear_allocator_t *allocator, linear_allocator_t *scratch, pathing_ranges_t *ranges, grid_t grid, position_t impact, int aoe_radius);

// Pops blast_preview_tiles back to an empty marker and clears
// blast_preview_valid. Cheaper than a full reset, and the required first
// step before staging a new preview via pathing_ranges_push_blast_preview.
PUBLIC void pathing_ranges_clear_blast_preview(linear_allocator_t *scratch, pathing_ranges_t *ranges);

#ifdef APP_UNITY_BUILD
#include "pathing_ranges.c"
#endif
