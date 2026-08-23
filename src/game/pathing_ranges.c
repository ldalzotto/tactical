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

// Adopts a caller-built walking_distances region, then re-pushes empty
// attack_range_tiles/blast_preview_tiles on top. Requires all three regions
// empty (see pathing_ranges_reset/pathing_ranges_init).
PRIVATE void pathing_ranges_set_walking_distances(linear_allocator_t *scratch, pathing_ranges_t *ranges, slice_t walking_distances_align, pathing_state_t walking_distances) {
    ranges->walking_distances_align = walking_distances_align;
    ranges->walking_distances = walking_distances;
    ranges->attack_range_align = linear_allocator_push(scratch, 0);
    ranges->attack_range_tiles = LINEAR_ALLOCATOR_PUSH(scratch, ranges->attack_range_tiles, 0);
    ranges->blast_preview_align = linear_allocator_push(scratch, 0);
    ranges->blast_preview_tiles = LINEAR_ALLOCATOR_PUSH(scratch, ranges->blast_preview_tiles, 0);
    ranges->blast_preview_valid = false;

    pathing_ranges_assert_layout(*ranges);
}

// Adopts a caller-built attack_range_tiles region, then re-pushes an empty
// blast_preview_tiles on top.
PRIVATE void pathing_ranges_set_attack_range(linear_allocator_t *scratch, pathing_ranges_t *ranges, slice_t attack_range_align, slice_position_t attack_range_tiles) {
    ranges->attack_range_align = attack_range_align;
    ranges->attack_range_tiles = attack_range_tiles;
    ranges->blast_preview_align = linear_allocator_push(scratch, 0);
    ranges->blast_preview_tiles = LINEAR_ALLOCATOR_PUSH(scratch, ranges->blast_preview_tiles, 0);
    ranges->blast_preview_valid = false;

    pathing_ranges_assert_layout(*ranges);
}

// Adopts a caller-built blast_preview_tiles region computed for `impact` and
// sets blast_preview_valid. Safe to call any time (unlike the setters above,
// doesn't require reachable/attack_range to be empty) as long as the
// previous preview was cleared first via pathing_ranges_clear_blast_preview.
PRIVATE void pathing_ranges_set_blast_preview(linear_allocator_t *scratch, pathing_ranges_t *ranges, slice_t blast_preview_align, slice_position_t blast_preview_tiles, position_t impact) {
    (void)scratch;
    ranges->blast_preview_align = blast_preview_align;
    ranges->blast_preview_tiles = blast_preview_tiles;
    ranges->blast_preview_impact = impact;
    ranges->blast_preview_valid = true;

    pathing_ranges_assert_layout(*ranges);
}

PUBLIC pathing_ranges_t pathing_ranges_init(linear_allocator_t *scratch) {
    slice_t walking_distances_align = linear_allocator_push(scratch, 0);
    pathing_state_t walking_distances = {
        .align = linear_allocator_push(scratch, 0),
        .dist = LINEAR_ALLOCATOR_PUSH(scratch, walking_distances.dist, 0),
    };
    slice_t attack_range_align = linear_allocator_push(scratch, 0);
    slice_position_t attack_range_tiles = LINEAR_ALLOCATOR_PUSH(scratch, attack_range_tiles, 0);
    slice_t blast_preview_align = linear_allocator_push(scratch, 0);
    slice_position_t blast_preview_tiles = LINEAR_ALLOCATOR_PUSH(scratch, blast_preview_tiles, 0);

    pathing_ranges_t ranges = {
        .walking_distances_align = walking_distances_align,
        .walking_distances = walking_distances,
        .attack_range_align = attack_range_align,
        .attack_range_tiles = attack_range_tiles,
        .blast_preview_align = blast_preview_align,
        .blast_preview_tiles = blast_preview_tiles,
    };

    pathing_ranges_assert_layout(ranges);

    return ranges;
}

PUBLIC void pathing_ranges_deinit(linear_allocator_t *scratch, pathing_ranges_t ranges) {
    LINEAR_ALLOCATOR_POP(scratch, ranges.blast_preview_tiles);
    linear_allocator_pop(scratch, ranges.blast_preview_align);
    LINEAR_ALLOCATOR_POP(scratch, ranges.attack_range_tiles);
    linear_allocator_pop(scratch, ranges.attack_range_align);
    LINEAR_ALLOCATOR_POP(scratch, ranges.walking_distances.dist);
    linear_allocator_pop(scratch, ranges.walking_distances.align);
    linear_allocator_pop(scratch, ranges.walking_distances_align);
}

PUBLIC void pathing_ranges_reset(linear_allocator_t *scratch, pathing_ranges_t *ranges) {
    pathing_ranges_deinit(scratch, *ranges);
    *ranges = pathing_ranges_init(scratch);
}

// Grows `scratch` in place if needed to fit `temp_tiles`, copies
// `temp_align`/`temp_tiles` into it as `out_align`/`out_tiles`, and pops the
// caller's staged copy off `allocator`. Growing inserts bytes into
// `allocator` right above `scratch`'s data, sliding up `temp` (the only
// thing staged there) -- rebases it. Returns the shift applied (0 if it
// already fit); callers must propagate it to anything else they hold above
// `scratch`.
PRIVATE ptrdiff_t pathing_ranges_push_tiles(linear_allocator_t *allocator, linear_allocator_t *scratch,
        slice_t temp_align, slice_position_t temp_tiles,
        slice_t *out_align, slice_position_t *out_tiles) {
    size_t needed = (size_t)SLICE_BYTESIZE(temp_tiles);
    size_t worst_case_padding = _Alignof(position_t) - 1;
    size_t available = (size_t)bytesize(scratch->cursor, scratch->data.end);
    size_t required = needed + worst_case_padding;

    ptrdiff_t shift = 0;
    if (required > available) {
        ptrdiff_t extra = (ptrdiff_t)(required - available);
        linear_allocator_insert(allocator, scratch->data.end, (size_t)extra);
        scratch->data.end = byteoffset(scratch->data.end, extra);

        temp_align = slice_shift(temp_align, extra);
        temp_tiles.slice = slice_shift(temp_tiles.slice, extra);

        shift = extra;
    }

    // Push data to the game scratch
    *out_align = linear_allocator_push_alignment(scratch, _Alignof(position_t));
    *out_tiles = LINEAR_ALLOCATOR_PUSH(scratch, temp_tiles, SLICE_TYPESIZE(temp_tiles));
    linear_allocator_copy(scratch, temp_tiles.slice, out_tiles->slice);

    linear_allocator_pop(allocator, temp_tiles.slice);
    linear_allocator_pop(allocator, temp_align);

    return shift;
}

// Grows `scratch` in place if needed to fit `temp`'s dist array, copies it
// in as `out`, then pops `temp` off `allocator`. `temp` is rebased in place
// if growth relocates memory. Returns the shift applied (0 if it already
// fit); callers must propagate it to anything else held above `scratch`.
PRIVATE ptrdiff_t game_scratch_push_pathing(linear_allocator_t *allocator, linear_allocator_t *scratch,
        pathing_state_t *temp,
        slice_t *out_align, pathing_state_t *out) {
    size_t needed = (size_t)SLICE_BYTESIZE(temp->dist);
    size_t worst_case_padding = _Alignof(int32_t) - 1;
    size_t available = (size_t)bytesize(scratch->cursor, scratch->data.end);
    size_t required = needed + worst_case_padding;

    ptrdiff_t shift = 0;
    if (required > available) {
        ptrdiff_t extra = (ptrdiff_t)(required - available);
        linear_allocator_insert(allocator, scratch->data.end, (size_t)extra);
        scratch->data.end = byteoffset(scratch->data.end, extra);

        temp->align = slice_shift(temp->align, extra);
        temp->dist.slice = slice_shift(temp->dist.slice, extra);

        shift = extra;
    }

    // Outer marker absorbs the real alignment padding, so pathing_state_t's
    // own internal align marker is pushed zero-length right after it.
    *out_align = linear_allocator_push_alignment(scratch, _Alignof(int32_t));
    out->align = linear_allocator_push(scratch, 0);
    out->dist = LINEAR_ALLOCATOR_PUSH(scratch, out->dist, SLICE_TYPESIZE(temp->dist));
    linear_allocator_copy(scratch, temp->dist.slice, out->dist.slice);

    linear_allocator_pop(allocator, temp->dist.slice);
    linear_allocator_pop(allocator, temp->align);

    return shift;
}

PUBLIC ptrdiff_t pathing_ranges_push_walking_distances(linear_allocator_t *allocator, linear_allocator_t *scratch, pathing_ranges_t *ranges, pathing_state_t *temp) {
    slice_t walking_distances_align;
    pathing_state_t walking_distances;
    ptrdiff_t shift = game_scratch_push_pathing(allocator, scratch, temp, &walking_distances_align, &walking_distances);

    pathing_ranges_set_walking_distances(scratch, ranges, walking_distances_align, walking_distances);

    return shift;
}

PUBLIC ptrdiff_t pathing_ranges_push_attack_range(linear_allocator_t *allocator, linear_allocator_t *scratch, pathing_ranges_t *ranges, slice_t temp_align, slice_position_t temp_tiles) {
    slice_t attack_range_align;
    slice_position_t attack_range_tiles;
    ptrdiff_t shift = pathing_ranges_push_tiles(allocator, scratch, temp_align, temp_tiles, &attack_range_align, &attack_range_tiles);

    pathing_ranges_set_attack_range(scratch, ranges, attack_range_align, attack_range_tiles);

    return shift;
}

PUBLIC ptrdiff_t pathing_ranges_push_blast_preview(linear_allocator_t *allocator, linear_allocator_t *scratch, pathing_ranges_t *ranges, grid_t grid, position_t impact, int aoe_radius) {
    slice_t temp_align = linear_allocator_push_alignment(allocator, _Alignof(position_t));
    slice_position_t temp_tiles = pathing_compute_blast_tiles(allocator, grid, impact, aoe_radius);

    slice_t blast_preview_align;
    slice_position_t blast_preview_tiles;
    ptrdiff_t shift = pathing_ranges_push_tiles(allocator, scratch, temp_align, temp_tiles, &blast_preview_align, &blast_preview_tiles);

    pathing_ranges_set_blast_preview(scratch, ranges, blast_preview_align, blast_preview_tiles, impact);

    return shift;
}

PUBLIC void pathing_ranges_clear_blast_preview(linear_allocator_t *scratch, pathing_ranges_t *ranges) {
    LINEAR_ALLOCATOR_POP(scratch, ranges->blast_preview_tiles);
    linear_allocator_pop(scratch, ranges->blast_preview_align);

    ranges->blast_preview_align = linear_allocator_push(scratch, 0);
    ranges->blast_preview_tiles = LINEAR_ALLOCATOR_PUSH(scratch, ranges->blast_preview_tiles, 0);
    ranges->blast_preview_valid = false;

    pathing_ranges_assert_layout(*ranges);
}
