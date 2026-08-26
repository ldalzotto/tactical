#pragma once

#include "../lib/linkage.h"

#include "../lib/memory.h"
#include "grid.h"
#include "pathing.h"
#include "position.h"

// Range data for the currently selected entity: action.c reads it to
// validate and resolve moves/attacks; render.c reads the same fields for
// overlays. Backed by three stacked regions in `scratch`, in this order:
// walking_distances, attack_range_tiles, blast_tiles (topmost).
//
// - walking_distances: raw BFS dist grid, valid only in GAME_MODE_MOVEMENT.
//   A tile is reachable iff its distance is >= 1 -- there's no separate
//   reachable-tiles list.
// - walking_distances vs. attack_range_tiles: mutually exclusive, matching
//   the move/attack mode toggle -- each collapses to an empty marker while
//   the other is active.
// - blast_tiles: independent of the pair above, can coexist with
//   attack_range_tiles (attack mode, AoE skill, valid hover). Kept synced to
//   the current hover/impact tile; game_cast_attack_area resolves the cast
//   against it directly, it's not just a rendering preview. Always
//   nullified in movement mode.
typedef struct {
    slice_t walking_distances_align;   // marker: alignment padding, or empty-region marker
    pathing_state_t walking_distances; // BFS dist grid; valid only in GAME_MODE_MOVEMENT
    slice_t attack_range_align;        // marker for attack_range_tiles
    slice_position_t attack_range_tiles; // tiles in the selected skill's range, attack mode only
    slice_t blast_align;               // marker for blast_tiles
    slice_position_t blast_tiles;      // AoE footprint for the tile it was last computed for, shared by cast resolution and rendering
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
// empty attack_range_tiles/blast_tiles on top. Call right after
// pathing_ranges_reset (requires all three regions empty). Returns the byte
// shift applied to `scratch` (0 if it already fit); callers must propagate
// it to anything else held above `scratch`, and `temp` is rebased in place.
PUBLIC ptrdiff_t pathing_ranges_push_walking_distances(linear_allocator_t *allocator, linear_allocator_t *scratch, pathing_ranges_t *ranges, pathing_state_t *temp);

// Same as pathing_ranges_push_walking_distances, but for attack_range_tiles
// (staged by the caller as `temp_align`/`temp_tiles`); re-pushes an empty
// blast_tiles on top.
PUBLIC ptrdiff_t pathing_ranges_push_attack_range(linear_allocator_t *allocator, linear_allocator_t *scratch, pathing_ranges_t *ranges, slice_t temp_align, slice_position_t temp_tiles);

// Computes the blast footprint centered on `impact`
// (pathing_compute_blast_tiles) and stages it into `scratch` as the new
// blast_tiles -- this is the data game_cast_attack_area resolves the cast
// against, not just a rendering preview. Unlike the pushes above, doesn't
// require walking_distances/attack_range_tiles to be empty, but the
// previous blast_tiles must be cleared first via
// pathing_ranges_clear_blast_tiles. Returns the byte shift applied (see
// pathing_ranges_push_walking_distances).
PUBLIC ptrdiff_t pathing_ranges_push_blast_tiles(linear_allocator_t *allocator, linear_allocator_t *scratch, pathing_ranges_t *ranges, grid_t grid, position_t impact, int aoe_radius);

// Pops blast_tiles back to an empty marker. Cheaper than a full reset, and
// the required first step before staging new blast_tiles via
// pathing_ranges_push_blast_tiles.
PUBLIC void pathing_ranges_clear_blast_tiles(linear_allocator_t *scratch, pathing_ranges_t *ranges);

#ifdef APP_UNITY_BUILD
#include "pathing_ranges.c"
#endif
