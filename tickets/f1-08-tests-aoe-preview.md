# F1-08: Tests — live blast preview

## Context
Part of F1. Covers the hover-driven preview behavior added in F1-06
(`game_update_blast_preview`, the third `render_cache` region from F1-05),
kept separate from F1-07 so it can be authored once F1-05/F1-06 land
without blocking on it. Follows the same suite conventions described in
F1-07's Context section (`src/test_game_helpers.h` helpers,
`test_tile_list_contains`, `assert_game_invariants`).

This can extend `src/test_game_aoe.c`/`.h` from F1-07 (same suite, more
cases) rather than a new file — implementer's call, but extending is
likely simpler since setup boilerplate (scenario with `SKILL_FIREBALL`,
attack mode + skill selection) is shared with F1-07's cases.

## Scope / implementation guidance
Use `test_move_to_pixel`/`test_move_tile` (existing hover-simulation
helpers per `test_game_helpers.h`) to drive `MOUSE_MOVE` events, then
inspect `game.render.blast_preview_tiles` (F1-05) via
`test_tile_list_contains`.

Test cases:

1. **Preview matches blast shape under hover.** Hovering a valid AoE
   impact tile populates `blast_preview_tiles` with exactly the tiles
   `pathing_compute_blast_tiles` would return for that tile (including
   cover-shadowing, reusing a wall/grass setup like F1-07's case 2).
2. **Preview updates as hover moves.** Moving hover from one valid tile to
   another recomputes the preview to match the new tile, not a stale union
   of both.
3. **Preview clears when hover leaves the grid.** `hover_valid == false`
   (e.g. mouse moved outside the viewport) empties `blast_preview_tiles`.
4. **Preview clears on mode change.** Toggling out of `GAME_MODE_ATTACK`
   (via `test_click_attack_toggle` or ending the turn) empties the preview,
   matching `render_cache_reset`'s existing clear-on-mode-change behavior.
5. **Preview clears on skill switch to non-AoE.** With an AoE skill
   selected and a populated preview, clicking a skill button
   (`test_click_skill_button`) to switch to `SKILL_MELEE`/`SKILL_RANGED`
   (`aoe_radius == 0`) empties the preview.
6. **Preview clears for out-of-range hover.** Hovering a tile beyond the
   attacker's `skill.range`/LOS leaves `blast_preview_tiles` empty (no
   partial/incorrect preview shown for an uncastable target).
7. **Preview coexists with attack range.** While a preview is populated,
   `game.render.attack_range_tiles` remains populated and correct
   (verifies F1-05's non-mutual-exclusivity requirement holds in practice,
   not just structurally).
8. **Preview computation is read-only.** Before/after a `MOUSE_MOVE` that
   populates a preview, attacker AP/HP/position and all entity
   AP/HP/positions are unchanged, and `assert_game_invariants` passes
   throughout (no game-state mutation from hovering alone).

## Acceptance criteria
- All 8 scenarios above pass.
- `src/test.c`/`CMakeLists.txt` registration is correct if a new file is
  used; if extending F1-07's `test_game_aoe.c`, no separate registration
  needed beyond what F1-07 already added.
- Existing suites remain unmodified and passing.

## Testing
This ticket *is* the testing work — see "Scope" above.

## Dependencies
- F1-05 (`render_cache` third region).
- F1-06 (`game_update_blast_preview` + rendering).
- F1-07 (if extending its file/suite rather than creating a new one).

## Non-goals
- No visual/pixel-level rendering assertions (this codebase's test
  convention checks `render_cache`/game state, not rasterized framebuffer
  output) — covering `graphics_draw_rectangle_dithered` output correctness
  is out of scope, matching how existing attack-range rendering isn't
  pixel-tested either.
