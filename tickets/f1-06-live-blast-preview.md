# F1-06: Live cover-aware blast preview on hover

## Context
Part of F1 — the "live preview" half of the feature. Hover state today is
minimal: `game->hover`/`game->hover_valid` (`src/game/game.h:44-45`),
updated only in the `MOUSE_MOVE` branch of `game_on_input_event`:

```c
// game.c:384-397 (MOUSE_MOVE branch)
int tx, ty;
bool valid = screen_to_grid(game->viewport, event.x, event.y, &tx, &ty);
game->hover_valid = valid;
if (valid) {
    game->hover = (position_t){ tx, ty };
}
```

It currently only drives a single-tile outline in `render.c:113-118`
(`if (game.hover_valid) { render_draw_outline(...); }`) — no game state is
recomputed on hover today. `game_set_mode` (`game.c:141-220`) is the
existing pattern for computing and staging an overlay tile list into
`game->scratch` via `game_scratch_push` (`game.c:90-124`, which grows the
scratch arena in place if needed and returns a `ptrdiff_t` byte shift that
every caller up the chain must propagate — see the comment at
`game.c:81-89` and its threading through `game_on_input_event`'s return
value per `game.h:67-69`).

This ticket wires a new computation, `game_update_blast_preview`, that
mirrors that pattern but is driven by hover movement (and skill selection)
rather than by an explicit mode switch, and adds the corresponding render
pass.

## Scope / implementation guidance
**Computation (`game.c`):**
- Add `PRIVATE ptrdiff_t game_update_blast_preview(game_state_t *game, linear_allocator_t *allocator)`:
  - Compute whether a preview should show:
    `game->mode == GAME_MODE_ATTACK && turn_active_entity(game->turn)->team == ENTITY_TEAM_PLAYER && SLICE_AT(active->skills, game->selected_skill).aoe_radius > 0 && game->hover_valid && pathing_in_range(game->grid, game->entities, active->position, game->hover, skill.range)`.
  - If false: clear `blast_preview_tiles` to empty (F1-05's clear/set-empty
    mutator) and return `0` (no scratch growth needed for the empty case).
  - If true: compute
    `pathing_compute_blast_tiles(allocator, game->grid, game->entities, game->hover, skill.aoe_radius)`
    (F1-02) staged on `allocator`, then grow/copy it into `game->scratch`
    via the existing `game_scratch_push` helper (`game.c:90-124`) — it's
    already generic over which cache field it targets (takes explicit
    `out_align`/`out_tiles` out-params), so it can be reused as-is here,
    passing `pathing = 0` (no `pathing_state_t` is staged alongside this
    call, unlike the `GAME_MODE_MOVEMENT` branch of `game_set_mode`).
    Adopt the result via F1-05's `render_cache_set_blast_preview`. Return
    the shift `game_scratch_push` reports.
- Call `game_update_blast_preview` from:
  - The `MOUSE_MOVE` branch of `game_on_input_event`, after updating
    `hover`/`hover_valid` — propagate its returned shift the same way this
    branch already needs to handle scratch growth (check how
    `game_on_input_event`'s other branches propagate `game_set_mode`'s
    shift, e.g. `game.c:260`, `game.c:273`, and apply the same discipline
    here since `MOUSE_MOVE` didn't previously call anything scratch-growing).
  - `game_on_skill_button_pressed` (`game.c:319-...`), after it updates
    `game->selected_skill`, so switching to/from an AoE skill refreshes the
    preview at the current hover tile without requiring the mouse to move.
  - `game_set_mode` (`game.c:141-220`) — clear the preview (call the
    "false" branch behavior) whenever mode changes, so toggling out of
    attack mode or switching to movement mode always clears it, matching
    `render_cache_reset`'s existing clear-both-on-mode-change behavior.
- This function must never mutate `attacker`/entity state (AP, HP,
  position) — it's read-only preview computation, same guarantee
  `game_set_mode` already provides for `reachable_tiles`/`attack_range_tiles`.

**Rendering (`render.c`):**
- Add a new tint constant near the existing overlay colors
  (`render.c:19-20`, `COLOR_REACHABLE_TINT`/`COLOR_ATTACK_RANGE_TINT`), e.g.
  `COLOR_BLAST_PREVIEW_TINT`, visually distinct from
  `COLOR_ATTACK_RANGE_TINT` so "what this hit would do" reads differently
  from "where I can target" (per the original design note).
- In `render_tiles` (`render.c:58-119`), add a third loop over
  `game.render.blast_preview_tiles`, drawn **after** the existing
  `attack_range_tiles` loop (`render.c:94-111`) so it layers on top, using
  `graphics_draw_rectangle_dithered` (matching the dithered style already
  used for targetable attack-range tiles at `render.c:107`) with the new
  tint.

## Acceptance criteria
- Hovering a valid AoE impact tile while an AoE skill is selected in
  attack mode shows the blast footprint (cover-shadowed correctly per
  F1-02) layered over the existing attack-range overlay.
- Moving the mouse to a different valid tile updates the preview; moving
  off-grid, switching to a non-AoE skill, or leaving attack mode clears it.
- The preview never changes `game->entities` state (AP/HP/position) or
  `game->turn` — purely visual.
- `ptrdiff_t` shifts from scratch growth during preview computation are
  correctly propagated wherever `game_on_input_event` needs to (no stale
  pointers into `game->scratch` after a `MOUSE_MOVE` that grows it).

## Testing
Add cases to F1-08 (or drive directly if simpler) covering:
- Preview tile list correctness for a given hover position (compare against
  expected blast tiles, reusing `test_tile_list_contains` from
  `src/test_game_helpers.h`).
- Preview clears on: hover leaving the grid, mode change out of `ATTACK`,
  skill switch to a non-AoE skill.
- Preview computation causes no AP/HP/position change (`assert_game_invariants`
  plus explicit before/after checks per `test_game_helpers.h` convention).

## Dependencies
- F1-01 (`skill_t.aoe_radius`).
- F1-02 (`pathing_compute_blast_tiles`).
- F1-05 (`render_cache` third region).

## Non-goals
- No changes to click-time casting behavior (F1-04) — this ticket is
  hover-only.
- No damage-falloff visualization.
