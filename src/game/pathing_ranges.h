#pragma once

#include "../lib/linkage.h"

#include "../lib/memory.h"
#include "grid.h"
#include "pathing.h"
#include "position.h"

// Range data for the currently selected entity: action.c reads it to
// validate/resolve moves and attacks; render.c reads the same fields for
// overlays. Backed by three stacked regions in `scratch`, in order:
// walking_distances, attack_range_tiles (+los_blocked_tiles), blast_tiles
// (topmost).
//
// - walking_distances: raw BFS dist grid, valid only in GAME_MODE_MOVEMENT.
//   Reachable iff distance >= 1 -- no separate reachable-tiles list.
// - walking_distances vs. attack_range_tiles: mutually exclusive, matching
//   the move/attack mode toggle -- each collapses to empty while the other
//   is active.
// - los_blocked_tiles: renderer-only, never read by action.c. Always
//   computed and cleared together with attack_range_tiles (see
//   pathing_compute_attack_range/pathing_ranges_push_attack_range), so it
//   shares attack_range_align and lives immediately after
//   attack_range_tiles with no gap -- the two are one physical allocation.
// - blast_tiles: independent of the pair above, can coexist with
//   attack_range_tiles (attack mode, AoE skill, valid hover). Synced to the
//   current hover/impact tile; game_cast_attack_area resolves the cast
//   against it directly, not just a rendering preview. Always nullified in
//   movement mode.
typedef struct {
    slice_t walking_distances_align;   // marker: alignment padding, or empty-region marker
    pathing_state_t walking_distances; // BFS dist grid; valid only in GAME_MODE_MOVEMENT
    slice_t attack_range_align;        // marker for attack_range_tiles/los_blocked_tiles
    slice_position_t attack_range_tiles; // tiles in the selected skill's range, attack mode only
    slice_position_t los_blocked_tiles;  // in range but LOS-blocked, attack mode only; renderer-only, contiguous right after attack_range_tiles
    slice_t blast_align;               // marker for blast_tiles
    slice_position_t blast_tiles;      // AoE footprint of the last-computed tile; shared by cast resolution and rendering
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

// Grows `scratch` in place if needed for `temp`'s dist array, copies it in
// as the new walking_distances, pops `temp` off `allocator`, and re-pushes
// empty attack_range_tiles/blast_tiles on top. Call right after
// pathing_ranges_reset (requires all three regions empty). Returns the byte
// shift applied (0 if it already fit); callers must propagate it to
// anything else held above `scratch`, and `temp` is rebased in place.
PUBLIC ptrdiff_t pathing_ranges_push_walking_distances(linear_allocator_t *allocator, linear_allocator_t *scratch, pathing_ranges_t *ranges, pathing_state_t *temp);

// Same as pathing_ranges_push_walking_distances, but for
// attack_range_tiles/los_blocked_tiles together (staged by the caller as
// `temp`, from pathing_compute_attack_range); re-pushes an empty
// blast_tiles on top.
PUBLIC ptrdiff_t pathing_ranges_push_attack_range(linear_allocator_t *allocator, linear_allocator_t *scratch, pathing_ranges_t *ranges, pathing_attack_range_t temp);

// Same as pathing_ranges_push_attack_range, but for blast_tiles (staged by
// the caller, e.g. via pathing_compute_blast_tiles) -- this is what
// game_cast_attack_area resolves the cast against, not just a rendering
// preview. Unlike the pushes above, doesn't require walking_distances/
// attack_range_tiles empty, but the previous blast_tiles must be cleared
// first via pathing_ranges_clear_blast_tiles.
PUBLIC ptrdiff_t pathing_ranges_push_blast_tiles(linear_allocator_t *allocator, linear_allocator_t *scratch, pathing_ranges_t *ranges, slice_t temp_align, slice_position_t temp_tiles);

// Pops blast_tiles back to an empty marker. Cheaper than a full reset, and
// the required first step before staging new blast_tiles via
// pathing_ranges_push_blast_tiles.
PUBLIC void pathing_ranges_clear_blast_tiles(linear_allocator_t *scratch, pathing_ranges_t *ranges);

#ifdef APP_UNITY_BUILD
#include "pathing_ranges.c"
#endif
