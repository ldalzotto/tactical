# F1-02: `pathing_compute_blast_tiles` — cover-aware blast footprint

## Context
Part of F1. The AoE blast footprint is a line-of-sight query rooted at the
impact tile: a tile is in the blast if it's within radius AND has a clear
sightline from the impact tile, so walls and sight-blocking terrain (tall
grass, `grid_blocks_sight`) automatically shape the blast without any new
geometry code.

The existing LOS primitives are point-to-point only, in
`src/game/pathing.h`/`pathing.c`:

- `pathing_line_of_sight_clear(grid_t grid, slice_entity_t entities, position_t from, position_t to)`
  (`pathing.c:114-129`) — walks the Bresenham ray via `geometry_line_iter_t`,
  excluding both endpoints; false if any intermediate tile blocks sight or
  is occupied.
- `pathing_in_range(grid_t grid, slice_entity_t entities, position_t from, position_t to, int max_range)`
  (`pathing.c:131-137`) — Manhattan distance check + `pathing_line_of_sight_clear`.

There is no grid-wide LOS precompute today (`pathing_state_t`,
`pathing.h:15-18`, only stores a flat walking-distance grid from
`pathing_compute_walking_distances`/`pathing_bfs`). The existing pattern for
"which tiles satisfy X from an origin" is a full-grid scan calling a
point-query per tile — see `game_set_mode`'s `GAME_MODE_ATTACK` branch,
`src/game/game.c:190-208`:

```c
for (int ty = 0; ty < game->grid.height; ty++) {
    for (int tx = 0; tx < game->grid.width; tx++) {
        position_t position = { tx, ty };
        if (position_equals(position, active->position)) continue;
        if (pathing_in_range(game->grid, game->entities, active->position, position, skill_range)) {
            slice_position_t entry = LINEAR_ALLOCATOR_PUSH(allocator, temp_tiles, 1);
            SLICE_DEREF(entry) = position;
            temp_tiles.end = entry.end;
        }
    }
}
```

This ticket adds a reusable helper following that same scan pattern, so both
the AoE action (F1-03) and the live blast preview (F1-06) can share it.

## Scope / implementation guidance
- Add to `src/game/pathing.h`:
  ```c
  PUBLIC slice_position_t pathing_compute_blast_tiles(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, position_t center, int radius);
  ```
- Implement in `src/game/pathing.c` following the scan style shown above:
  stage a growable `slice_position_t` on `allocator` (same
  `linear_allocator_push_alignment` + `LINEAR_ALLOCATOR_PUSH` +
  `SLICE_DEREF`/`.end` growth pattern used in `game.c`), loop every grid
  tile, and include a tile if:
  - Manhattan distance from `center` is `<= radius`, AND
  - the tile equals `center`, OR `pathing_line_of_sight_clear(grid, entities, center, tile)` is true.

  (Manhattan distance is computed today via the `PRIVATE` helper
  `pathing_manhattan_distance` in `pathing.c:104-108` — either reuse it
  in-file or add an equivalent local check; it's not exported.)
- The returned slice is a plain tile list (no per-tile distance value) —
  intentionally simpler than `pathing_state_t`; damage falloff by distance
  is an explicit non-goal for this iteration (see F1-01's non-goals).
- Caller owns the returned slice and must pop it from `allocator` when done
  (same convention as every other allocator-staged temp in this codebase —
  see `pathing_deinit` for the analogous teardown of `pathing_state_t`,
  though this new function does *not* need its own `_deinit`, since the
  result is a single `slice_position_t`, not a struct with padding —
  callers can just `linear_allocator_pop(allocator, result.slice)` directly
  like the tile lists in `game.c` do).

## Acceptance criteria
- `pathing_compute_blast_tiles` returns exactly the tiles within `radius`
  of `center` that have a clear sightline from `center` (or are `center`
  itself), matching `pathing_line_of_sight_clear`'s existing
  wall/sight-blocking-terrain/occupied-tile semantics.
- A wall or sight-blocking tile (e.g. `TILE_GRASS`, see
  `grid_blocks_sight`) between `center` and a tile beyond it removes that
  farther tile from the result, even if within radius (i.e. the blast
  "shadow" behind cover is respected).
- Function works correctly for `radius == 0` (returns just `center`, if
  in bounds) and for `center` at/near grid edges.

## Testing
- Given the codebase's convention of driving pathing behavior only through
  the public game API in tests (no dedicated `test_pathing.c` exists today —
  `pathing_` symbols are exercised indirectly via `test_game_movement.c`),
  this ticket's correctness is primarily verified through F1-07's
  functional AoE tests (which exercise the blast shape via
  `action_try_attack_area`). If the implementer wants tighter unit coverage
  of the LOS-shadowing behavior specifically, adding cases to
  `test_game_movement.c` or a small `test_pathing.c` is acceptable but not
  required by this ticket.

## Dependencies
None (standalone addition to `pathing.c`/`pathing.h`; does not depend on
F1-01).

## Non-goals
- No per-tile distance/falloff data.
- No new `pathing_state_t`-style struct or `_deinit` function.
