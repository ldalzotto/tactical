#include "pathing_ranges.h"

#include "../lib/assert.h"

// Enforces the region stacking order in scratch (walking_distances <
// attack_range_tiles < blast_preview_tiles). Every mutator below calls this
// before returning, so a reorder bug trips an assert instead of silently
// corrupting another region.
PRIVATE void pathing_ranges_assert_layout(pathing_ranges_t ranges) {
    assert_debug(ranges.attack_range_align.begin >= ranges.walking_distances.dist.slice.end);
    assert_debug((void*)ranges.attack_range_tiles.begin >= ranges.walking_distances.dist.slice.end);
    assert_debug(ranges.blast_preview_align.begin >= ranges.attack_range_tiles.slice.end);
    assert_debug((void*)ranges.blast_preview_tiles.begin >= ranges.attack_range_tiles.slice.end);
}

PUBLIC void pathing_ranges_reset(linear_allocator_t *scratch, pathing_ranges_t *ranges) {
    LINEAR_ALLOCATOR_POP(scratch, ranges->blast_preview_tiles);
    linear_allocator_pop(scratch, ranges->blast_preview_align);
    LINEAR_ALLOCATOR_POP(scratch, ranges->attack_range_tiles);
    linear_allocator_pop(scratch, ranges->attack_range_align);
    LINEAR_ALLOCATOR_POP(scratch, ranges->walking_distances.dist);
    linear_allocator_pop(scratch, ranges->walking_distances.align);
    linear_allocator_pop(scratch, ranges->walking_distances_align);

    ranges->walking_distances_align = linear_allocator_push(scratch, 0);
    ranges->walking_distances.align = linear_allocator_push(scratch, 0);
    ranges->walking_distances.dist = LINEAR_ALLOCATOR_PUSH(scratch, ranges->walking_distances.dist, 0);
    ranges->attack_range_align = linear_allocator_push(scratch, 0);
    ranges->attack_range_tiles = LINEAR_ALLOCATOR_PUSH(scratch, ranges->attack_range_tiles, 0);
    ranges->blast_preview_align = linear_allocator_push(scratch, 0);
    ranges->blast_preview_tiles = LINEAR_ALLOCATOR_PUSH(scratch, ranges->blast_preview_tiles, 0);
    ranges->blast_preview_valid = false;

    pathing_ranges_assert_layout(*ranges);
}

PUBLIC void pathing_ranges_set_walking_distances(linear_allocator_t *scratch, pathing_ranges_t *ranges, slice_t walking_distances_align, pathing_state_t walking_distances) {
    ranges->walking_distances_align = walking_distances_align;
    ranges->walking_distances = walking_distances;
    ranges->attack_range_align = linear_allocator_push(scratch, 0);
    ranges->attack_range_tiles = LINEAR_ALLOCATOR_PUSH(scratch, ranges->attack_range_tiles, 0);
    ranges->blast_preview_align = linear_allocator_push(scratch, 0);
    ranges->blast_preview_tiles = LINEAR_ALLOCATOR_PUSH(scratch, ranges->blast_preview_tiles, 0);
    ranges->blast_preview_valid = false;

    pathing_ranges_assert_layout(*ranges);
}

PUBLIC void pathing_ranges_set_attack_range(linear_allocator_t *scratch, pathing_ranges_t *ranges, slice_t attack_range_align, slice_position_t attack_range_tiles) {
    ranges->attack_range_align = attack_range_align;
    ranges->attack_range_tiles = attack_range_tiles;
    ranges->blast_preview_align = linear_allocator_push(scratch, 0);
    ranges->blast_preview_tiles = LINEAR_ALLOCATOR_PUSH(scratch, ranges->blast_preview_tiles, 0);
    ranges->blast_preview_valid = false;

    pathing_ranges_assert_layout(*ranges);
}

PUBLIC void pathing_ranges_set_blast_preview(linear_allocator_t *scratch, pathing_ranges_t *ranges, slice_t blast_preview_align, slice_position_t blast_preview_tiles, position_t impact) {
    (void)scratch;
    ranges->blast_preview_align = blast_preview_align;
    ranges->blast_preview_tiles = blast_preview_tiles;
    ranges->blast_preview_impact = impact;
    ranges->blast_preview_valid = true;

    pathing_ranges_assert_layout(*ranges);
}

PUBLIC void pathing_ranges_clear_blast_preview(linear_allocator_t *scratch, pathing_ranges_t *ranges) {
    LINEAR_ALLOCATOR_POP(scratch, ranges->blast_preview_tiles);
    linear_allocator_pop(scratch, ranges->blast_preview_align);

    ranges->blast_preview_align = linear_allocator_push(scratch, 0);
    ranges->blast_preview_tiles = LINEAR_ALLOCATOR_PUSH(scratch, ranges->blast_preview_tiles, 0);
    ranges->blast_preview_valid = false;

    pathing_ranges_assert_layout(*ranges);
}
