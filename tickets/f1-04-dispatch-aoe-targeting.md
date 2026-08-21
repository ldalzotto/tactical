# F1-04: Dispatch AoE casts from tile/entity clicks

## Context
Part of F1. Targeting today is strictly entity-click-based. The single
input dispatcher, `game_on_input_event` (`src/game/game.c:348-397`),
classifies each click and routes it:

```c
// game.c:372-383
entity_t* found = entity_find_at(game->entities, target);
if (found != 0) {
    return game_on_entity_pressed(game, allocator, found);
} else {
    return game_on_tile_pressed(game, allocator, target);
}
```

`game_on_entity_pressed` (`game.c:248-277`) is the only path that currently
calls an attack action, and only against the clicked entity:

```c
if (action_try_attack(game->grid, game->entities, active, SLICE_AT(active->skills, game->selected_skill), entity)) {
    if (!entity->alive) {
        game->turn = turn_remove_dead_entity(game->turn, entity);
    }
    game_check_game_over(game);
    return game_set_mode(game, allocator, GAME_MODE_MOVEMENT);
}
```

`game_on_tile_pressed` (`game.c:279-300`) currently has **no attack-mode
behavior at all** — it early-returns unless `game->mode == GAME_MODE_MOVEMENT`,
and asserts the clicked tile is unoccupied (since dispatch already routed
occupied tiles to the entity-press path).

Per the confirmed plan, AoE skills are targeted by clicking **any** in-range
tile (empty or occupied) — that tile becomes the blast center. Single-target
skills (`aoe_radius == 0`) keep today's entity-only-click behavior
unchanged.

## Scope / implementation guidance
- In both `game_on_entity_pressed` and `game_on_tile_pressed`, before the
  existing attack logic, check
  `SLICE_AT(active->skills, game->selected_skill).aoe_radius > 0` while
  `game->mode == GAME_MODE_ATTACK` and `active->team == ENTITY_TEAM_PLAYER`.
  When true, treat the clicked position as the impact tile and call
  `action_try_attack_area` (F1-03) instead of `action_try_attack`:
  - `game_on_entity_pressed`: use `entity->position` as the impact tile
    instead of passing `entity` directly to `action_try_attack`.
  - `game_on_tile_pressed`: drop (or scope down) the existing
    `assert_debug(entity_find_at(game->entities, target) == 0)` so it only
    applies to the non-AoE path — an AoE cast can legally target an empty
    tile here now. Use `target` as the impact tile.
- On a successful `action_try_attack_area` call in either handler: loop the
  `out_hit` list, and for every entity where `!entity->alive`, call
  `game->turn = turn_remove_dead_entity(game->turn, entity)` (mirrors the
  single-kill pattern at `game.c:269-270` / `game.c:235-236`, just looped).
  Call `game_check_game_over(game)` **once**, after the loop finishes (not
  per-casualty) — `game_check_game_over` asserts
  `game->game_over == GAME_OVER_NONE` on entry (`game.c:10`), so it must
  only be called when it hasn't already been set this call. Then pop
  `out_hit` from `allocator`, and return
  `game_set_mode(game, allocator, GAME_MODE_MOVEMENT)` same as the existing
  single-target path.
- Consider factoring the shared "resolve impact tile → call
  action_try_attack_area → reconcile turn order → check game over" logic
  into one `PRIVATE` helper called from both handlers, to avoid duplicating
  the turn-removal loop — follow this file's existing style of small
  `PRIVATE` handler functions (e.g. `game_on_attack_toggle_pressed`,
  `game.c:302-313`).
- Since `active->team` can never appear in `out_hit` (F1-03 excludes
  same-team damage), the active entity is guaranteed never among the
  casualties reconciled here — no special-casing of
  `turn_remove_dead_entity`'s `dead != active` assert is needed.

## Acceptance criteria
- With an AoE skill selected in attack mode, clicking any tile within the
  attacker's skill range/LOS (empty or occupied) casts the AoE skill
  centered on that tile.
- With a non-AoE skill selected, behavior is unchanged: only clicking an
  enemy entity attacks it; clicking an empty tile in attack mode still
  no-ops (falls through to `GAME_MODE_MOVEMENT`'s existing tile-click
  behavior only when mode is actually `MOVEMENT`).
- A blast that kills N entities in one cast removes all N from
  `game->turn.order` correctly (verified via `test_game_helpers.h`-style
  assertions in F1-07), and leaves `game->turn.cursor` pointing at a valid
  entity.
- A blast that wipes the last enemy (or, if reachable, player) sets
  `game->game_over` correctly exactly once.
- Clicking an out-of-range/LOS-blocked tile with an AoE skill selected does
  nothing (no AP spent, no state change) — mirrors `action_try_attack`
  returning `false` today.

## Testing
Covered primarily by F1-07 (functional AoE tests via
`game_on_input_event`/`test_game_helpers.h` click wrappers, e.g.
`test_click_tile`, `test_click_skill_button`). Add/extend cases there for:
multi-kill turn-order reconciliation, multi-kill game-over, AoE cast onto
an empty tile, and the out-of-range no-op case.

## Dependencies
- F1-01 (`skill_t.aoe_radius`).
- F1-03 (`action_try_attack_area`).

## Non-goals
- No changes to `game_on_attack_toggle_pressed` or
  `game_on_skill_button_pressed` beyond what F1-06 separately needs for
  preview refresh — this ticket is scoped to click-time casting only, not
  hover preview.
