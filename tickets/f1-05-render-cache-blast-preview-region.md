# F1-05: Third `render_cache` region for the blast preview

## Context
Part of F1. `render_cache_t` (`src/game/render_cache.h:15-24`) currently has
exactly two overlay regions, explicitly documented as mutually exclusive
and stacked in a fixed order in `game->scratch`:

```c
// render_cache.h:8-14
// - reachable_tiles and attack_range_tiles are mutually exclusive: exactly
//   one is populated at a time (the other nullified), matching the
//   move/attack toggle -- scratch only ever holds one live tile cache.
// - stacked regions of the same scratch arena: attack_range_tiles must
//   always sit after reachable_tiles in memory.
typedef struct {
    slice_t reachable_align;
    slice_position_t reachable_tiles;
    slice_t attack_range_align;
    slice_position_t attack_range_tiles;
} render_cache_t;
```

Mutators enforce this: `render_cache_reset` (pops both, pushes two empty
markers), `render_cache_set_reachable` (requires both empty, then re-pushes
an empty `attack_range_tiles` on top so it stays topmost),
`render_cache_set_attack_range` (requires `reachable_tiles` already empty).
`render_cache_assert_layout` (`render_cache.c:8-11`) enforces the stacking
order.

Per the confirmed plan, the blast preview must render **simultaneously**
with `attack_range_tiles` (attack range stays visible while the hovered
blast footprint draws on top) — this is a third, independently-toggled
region, not a replacement for the existing mutual exclusivity between
`reachable_tiles` and `attack_range_tiles` (which is unaffected and must
keep working exactly as today).

This ticket only adds the data structure and its mutators/reset logic — no
consumer wiring. F1-06 computes and populates it; the render loop consuming
it is also added in F1-06 (`render.c`) since it's a small, cohesive change
alongside the computation trigger.

## Scope / implementation guidance
- Add a third stacked region to `render_cache_t`, always topmost (after
  `attack_range_tiles`):
  ```c
  slice_t blast_preview_align;
  slice_position_t blast_preview_tiles;
  ```
- Update the struct's doc comment: `reachable_tiles`/`attack_range_tiles`
  remain mutually exclusive as today; `blast_preview_tiles` is independent
  — it can be populated at the same time as `attack_range_tiles` (while
  `attack_range_tiles` is populated and the hovered tile is a valid AoE
  impact), and is always nullified whenever `reachable_tiles` is populated
  (movement mode has no attack preview).
- Extend `render_cache_assert_layout` (`render_cache.c:8-11`) to check the
  third region's stacking position.
- Extend `render_cache_reset` to pop/re-push all three regions (currently
  pops both existing ones then pushes two empty markers — add the third).
- Add `render_cache_set_blast_preview(linear_allocator_t *scratch, render_cache_t *cache, slice_t blast_preview_align, slice_position_t blast_preview_tiles)`:
  unlike `render_cache_set_reachable`/`render_cache_set_attack_range`, this
  does **not** require the other two regions to be empty — it's always
  callable whenever `attack_range_tiles`'s region exists (populated or
  empty-marker) beneath it, since it's always the topmost region regardless
  of the other two's state.
- Since `blast_preview_tiles` recomputes on every hover move (F1-06), it
  needs to be cheaply clearable to empty without requiring a full
  `render_cache_reset` of the other two regions — provide a way to set it
  to a zero-length list (either `render_cache_set_blast_preview` called
  with an empty slice, or a dedicated
  `render_cache_clear_blast_preview(linear_allocator_t *scratch, render_cache_t *cache)` —
  implementer's choice, whichever reads more consistently with the
  existing `render_cache_reset`/`_set_*` naming).

## Acceptance criteria
- `render_cache_t` has a third `blast_preview_align`/`blast_preview_tiles`
  pair, stacked topmost.
- `render_cache_reset` clears all three regions to empty markers.
- `render_cache_set_reachable`/`render_cache_set_attack_range` behavior is
  unchanged (still mutually exclusive with each other); their existing
  preconditions/postconditions still hold with the third region present.
- A new mutator allows populating/clearing `blast_preview_tiles`
  independently of the other two, without violating
  `render_cache_assert_layout`.
- `game_init`/`game_deinit` (`game.c:22-79`) are updated to push/pop the
  third scratch region alongside the existing two (see
  `game.c:33-36` for init, `game.c:64-69` for deinit) so scratch teardown
  stays balanced.

## Testing
- No new test file required for this ticket alone (it's pure
  infrastructure with no behavior visible through the game API yet).
  `render_cache_assert_layout` firing correctly under
  `assert_debug`-enabled builds is the main correctness signal; F1-08's
  preview tests exercise this indirectly once F1-06 wires up a producer.
- Ensure existing tests (`test_game_movement.c`, `test_game_combat.c`,
  `test_render.c`) still pass unmodified — the mutual exclusivity of
  `reachable_tiles`/`attack_range_tiles` must be fully preserved.

## Dependencies
None (structural change to `render_cache.h`/`.c` and `game_init`/`game_deinit`;
independent of F1-01 through F1-04).

## Non-goals
- No computation of blast preview tiles (F1-06).
- No rendering of the new region (F1-06).
