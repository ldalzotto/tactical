#pragma once

#include "../lib/linkage.h"

#include "../lib/memory.h"
#include "pathing.h"
#include "position.h"

// Range data for the currently selected entity: rendering reads it for
// overlays, action.c reads it to validate moves/attacks. Backed by four
// stacked regions in `scratch`, in this order: walking_distances,
// reachable_tiles, attack_range_tiles, blast_preview_tiles (topmost).
//
// - walking_distances: raw BFS dist grid, valid only in GAME_MODE_MOVEMENT.
//   action_try_move queries it instead of re-running the BFS.
// - reachable_tiles vs. attack_range_tiles: mutually exclusive, matching
//   the move/attack mode toggle (whichever isn't active is nullified).
// - blast_preview_tiles: independent of the pair above. Can coexist with
//   attack_range_tiles (attack mode, AoE skill, valid hover), layering the
//   blast preview on top of the range overlay. Always nullified in
//   movement mode.
typedef struct {
    slice_t walking_distances_align;   // marker: alignment padding, or empty-region marker
    pathing_state_t walking_distances; // BFS dist grid; valid only in GAME_MODE_MOVEMENT
    slice_t reachable_align;           // marker for reachable_tiles
    slice_position_t reachable_tiles;  // tiles the selected entity can reach this turn
    slice_t attack_range_align;        // marker for attack_range_tiles
    slice_position_t attack_range_tiles; // tiles in the selected skill's range, attack mode only
    slice_t blast_preview_align;       // marker for blast_preview_tiles
    slice_position_t blast_preview_tiles; // AoE footprint under the current hover
    position_t blast_preview_impact;   // tile blast_preview_tiles was computed for
    bool blast_preview_valid;          // true iff blast_preview_tiles/_impact hold real data
} pathing_ranges_t;

// Collapses all four regions back to empty markers at the pre-selection watermark.
PUBLIC void pathing_ranges_reset(linear_allocator_t *scratch, pathing_ranges_t *ranges);

// Adopts caller-built walking_distances + reachable_tiles regions, then
// re-pushes empty attack_range_tiles/blast_preview_tiles on top. Call right
// after pathing_ranges_reset (requires all four regions empty).
PUBLIC void pathing_ranges_set_reachable(linear_allocator_t *scratch, pathing_ranges_t *ranges, slice_t walking_distances_align, pathing_state_t walking_distances, slice_t reachable_align, slice_position_t reachable_tiles);

// Adopts a caller-built attack_range_tiles region, then re-pushes an empty
// blast_preview_tiles on top. Call right after pathing_ranges_reset.
PUBLIC void pathing_ranges_set_attack_range(linear_allocator_t *scratch, pathing_ranges_t *ranges, slice_t attack_range_align, slice_position_t attack_range_tiles);

// Adopts a caller-built blast_preview_tiles region computed for `impact`
// and sets blast_preview_valid. Safe to call any time (unlike the setters
// above, doesn't require reachable/attack_range to be empty) as long as
// the previous preview was cleared first via pathing_ranges_clear_blast_preview.
PUBLIC void pathing_ranges_set_blast_preview(linear_allocator_t *scratch, pathing_ranges_t *ranges, slice_t blast_preview_align, slice_position_t blast_preview_tiles, position_t impact);

// Pops blast_preview_tiles back to an empty marker and clears
// blast_preview_valid. Cheaper than a full reset, and the required first
// step before staging a new preview via pathing_ranges_set_blast_preview.
PUBLIC void pathing_ranges_clear_blast_preview(linear_allocator_t *scratch, pathing_ranges_t *ranges);

#ifdef APP_UNITY_BUILD
#include "pathing_ranges.c"
#endif
