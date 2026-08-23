#include "pathing_cache.h"

#include "../lib/assert.h"

// reachable_tiles must always sit after walking_distances, attack_range_tiles
// must always sit after reachable_tiles, and blast_preview_tiles must always
// sit after attack_range_tiles, in scratch. Every mutator below re-checks
// this before returning, so a stray reorder trips an assert instead of
// silently corrupting another cache.
PRIVATE void pathing_cache_assert_layout(pathing_cache_t cache) {
    assert_debug(cache.reachable_align.begin >= cache.walking_distances.dist.slice.end);
    assert_debug((void*)cache.reachable_tiles.begin >= cache.walking_distances.dist.slice.end);
    assert_debug(cache.attack_range_align.begin >= cache.reachable_tiles.slice.end);
    assert_debug((void*)cache.attack_range_tiles.begin >= cache.reachable_tiles.slice.end);
    assert_debug(cache.blast_preview_align.begin >= cache.attack_range_tiles.slice.end);
    assert_debug((void*)cache.blast_preview_tiles.begin >= cache.attack_range_tiles.slice.end);
}

PUBLIC void pathing_cache_reset(linear_allocator_t *scratch, pathing_cache_t *cache) {
    LINEAR_ALLOCATOR_POP(scratch, cache->blast_preview_tiles);
    linear_allocator_pop(scratch, cache->blast_preview_align);
    LINEAR_ALLOCATOR_POP(scratch, cache->attack_range_tiles);
    linear_allocator_pop(scratch, cache->attack_range_align);
    LINEAR_ALLOCATOR_POP(scratch, cache->reachable_tiles);
    linear_allocator_pop(scratch, cache->reachable_align);
    LINEAR_ALLOCATOR_POP(scratch, cache->walking_distances.dist);
    linear_allocator_pop(scratch, cache->walking_distances.align);
    linear_allocator_pop(scratch, cache->walking_distances_align);

    cache->walking_distances_align = linear_allocator_push(scratch, 0);
    cache->walking_distances.align = linear_allocator_push(scratch, 0);
    cache->walking_distances.dist = LINEAR_ALLOCATOR_PUSH(scratch, cache->walking_distances.dist, 0);
    cache->reachable_align = linear_allocator_push(scratch, 0);
    cache->reachable_tiles = LINEAR_ALLOCATOR_PUSH(scratch, cache->reachable_tiles, 0);
    cache->attack_range_align = linear_allocator_push(scratch, 0);
    cache->attack_range_tiles = LINEAR_ALLOCATOR_PUSH(scratch, cache->attack_range_tiles, 0);
    cache->blast_preview_align = linear_allocator_push(scratch, 0);
    cache->blast_preview_tiles = LINEAR_ALLOCATOR_PUSH(scratch, cache->blast_preview_tiles, 0);

    pathing_cache_assert_layout(*cache);
}

PUBLIC void pathing_cache_set_reachable(linear_allocator_t *scratch, pathing_cache_t *cache, slice_t walking_distances_align, pathing_state_t walking_distances, slice_t reachable_align, slice_position_t reachable_tiles) {
    cache->walking_distances_align = walking_distances_align;
    cache->walking_distances = walking_distances;
    cache->reachable_align = reachable_align;
    cache->reachable_tiles = reachable_tiles;
    cache->attack_range_align = linear_allocator_push(scratch, 0);
    cache->attack_range_tiles = LINEAR_ALLOCATOR_PUSH(scratch, cache->attack_range_tiles, 0);
    cache->blast_preview_align = linear_allocator_push(scratch, 0);
    cache->blast_preview_tiles = LINEAR_ALLOCATOR_PUSH(scratch, cache->blast_preview_tiles, 0);

    pathing_cache_assert_layout(*cache);
}

PUBLIC void pathing_cache_set_attack_range(linear_allocator_t *scratch, pathing_cache_t *cache, slice_t attack_range_align, slice_position_t attack_range_tiles) {
    cache->attack_range_align = attack_range_align;
    cache->attack_range_tiles = attack_range_tiles;
    cache->blast_preview_align = linear_allocator_push(scratch, 0);
    cache->blast_preview_tiles = LINEAR_ALLOCATOR_PUSH(scratch, cache->blast_preview_tiles, 0);

    pathing_cache_assert_layout(*cache);
}

PUBLIC void pathing_cache_set_blast_preview(linear_allocator_t *scratch, pathing_cache_t *cache, slice_t blast_preview_align, slice_position_t blast_preview_tiles) {
    (void)scratch;
    cache->blast_preview_align = blast_preview_align;
    cache->blast_preview_tiles = blast_preview_tiles;

    pathing_cache_assert_layout(*cache);
}

PUBLIC void pathing_cache_clear_blast_preview(linear_allocator_t *scratch, pathing_cache_t *cache) {
    LINEAR_ALLOCATOR_POP(scratch, cache->blast_preview_tiles);
    linear_allocator_pop(scratch, cache->blast_preview_align);

    cache->blast_preview_align = linear_allocator_push(scratch, 0);
    cache->blast_preview_tiles = LINEAR_ALLOCATOR_PUSH(scratch, cache->blast_preview_tiles, 0);

    pathing_cache_assert_layout(*cache);
}
