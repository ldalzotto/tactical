#include "render_cache.h"

#include "../lib/assert.h"

// reachable_tiles and attack_range_tiles are mutually exclusive, stacked
// regions of the same scratch arena: attack_range_tiles must always sit
// after reachable_tiles in memory. Every mutator below re-checks that
// layout before returning, so a stray reorder trips an assert instead of
// silently corrupting the other cache.
PRIVATE void render_cache_assert_layout(render_cache_t cache) {
    assert_debug(cache.attack_range_align.begin >= cache.reachable_tiles.end);
    assert_debug(cache.attack_range_tiles.begin >= cache.reachable_tiles.end);
}

PUBLIC void render_cache_reset(linear_allocator_t *scratch, render_cache_t *cache) {
    LINEAR_ALLOCATOR_POP(scratch, cache->attack_range_tiles);
    linear_allocator_pop(scratch, cache->attack_range_align);
    LINEAR_ALLOCATOR_POP(scratch, cache->reachable_tiles);
    linear_allocator_pop(scratch, cache->reachable_align);

    cache->reachable_align = linear_allocator_push(scratch, 0);
    cache->reachable_tiles = LINEAR_ALLOCATOR_PUSH(scratch, cache->reachable_tiles, 0);
    cache->attack_range_align = linear_allocator_push(scratch, 0);
    cache->attack_range_tiles = LINEAR_ALLOCATOR_PUSH(scratch, cache->attack_range_tiles, 0);

    render_cache_assert_layout(*cache);
}

PUBLIC void render_cache_write_reachable(linear_allocator_t *scratch, render_cache_t *cache, int count) {
    LINEAR_ALLOCATOR_POP(scratch, cache->attack_range_tiles);
    linear_allocator_pop(scratch, cache->attack_range_align);
    LINEAR_ALLOCATOR_POP(scratch, cache->reachable_tiles);
    linear_allocator_pop(scratch, cache->reachable_align);

    cache->reachable_align = linear_allocator_push_alignment(scratch, _Alignof(position_t));
    cache->reachable_tiles = LINEAR_ALLOCATOR_PUSH(scratch, cache->reachable_tiles, count);
    cache->attack_range_align = linear_allocator_push(scratch, 0);
    cache->attack_range_tiles = LINEAR_ALLOCATOR_PUSH(scratch, cache->attack_range_tiles, 0);

    render_cache_assert_layout(*cache);
}

PUBLIC void render_cache_write_attack_range(linear_allocator_t *scratch, render_cache_t *cache, int count) {
    LINEAR_ALLOCATOR_POP(scratch, cache->attack_range_tiles);
    linear_allocator_pop(scratch, cache->attack_range_align);

    cache->attack_range_align = linear_allocator_push_alignment(scratch, _Alignof(position_t));
    cache->attack_range_tiles = LINEAR_ALLOCATOR_PUSH(scratch, cache->attack_range_tiles, count);

    render_cache_assert_layout(*cache);
}
