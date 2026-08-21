# F1-07: Functional tests — AoE casting, blast shape, turn order, game over

## Context
Part of F1. This codebase tests actions/pathing exclusively through the
public game API, never by calling `action_*`/`pathing_*` functions
directly — see the convention comment at the top of
`src/test_game_movement.c:9-16` and the mirrored structure in
`src/test_game_combat.c`. Suites are `src/test_<area>.c/.h` pairs exposing
`extern const test_case_t g_<suite>_tests[]` /
`g_<suite>_tests_count`, registered in `src/test.c`'s `test_lookup`
if-chain, and added to `CMakeLists.txt`'s explicit test source list
(`CMakeLists.txt:84-91` area — no glob, files must be listed by name).

Shared helpers live in `src/test_game_helpers.h`: grid/entity/skill/turn
setup boilerplate (see the pattern used at the top of
`test_game_combat.c`/`test_game_movement.c` — manual
`grid_align`/`grid_init`, `entity_list_align`/`entity_list_init`/`entity_spawn`,
`skill_list_align`/`skill_list_init`/`skill_list_add`,
`turn_order_align`/`turn_order_init`/`turn_order_add`, then `game_init`),
plus input-simulation wrappers: `test_click_tile`, `test_move_to_pixel`,
`test_move_tile`, `test_click_end_turn`, `test_click_attack_toggle`,
`test_click_skill_button`, and `test_tile_list_contains(slice_position_t tiles, position_t target)`.
Every helper calls `assert_game_invariants` (`test_invariants.h`) after
each simulated input.

This ticket covers the click-to-cast / damage-application / turn-order /
game-over behavior from F1-03 and F1-04. Hover-preview behavior is F1-08,
kept separate so it can be written/landed independently once F1-05/F1-06
are done, without blocking on this ticket.

## Scope / implementation guidance
Create `src/test_game_aoe.c` and `src/test_game_aoe.h`, following the
existing suite pattern (model directly on `test_game_combat.c`'s
structure: setup helper, one `test_case_t` per behavior, `g_..._tests`/
`g_..._tests_count` exports).

Register the new suite:
- Add the `#include`/`test_lookup` wiring in `src/test.c` (follow how the
  existing suites, e.g. `test_game_combat`, are registered there).
- Add `src/test_game_aoe.c` to `CMakeLists.txt`'s test source list
  alongside the other `src/test_game_*.c` entries.

Test cases to cover, each built via a scenario with `SKILL_FIREBALL`
(`range=4, aoe_radius=2, damage=4`, from F1-01) given to the acting entity,
using `test_click_skill_button`/`test_click_attack_toggle` to select attack
mode and the AoE skill, then `test_click_tile` (F1-04's new AoE tile-click
path) to cast:

1. **Blast hits all enemies in radius.** Multiple enemy entities placed
   within `aoe_radius` of an impact tile all take `skill.damage`; an enemy
   entity just outside the radius is untouched.
2. **Cover shadows the blast.** A wall or `TILE_GRASS` tile between the
   impact center and an enemy within Manhattan radius blocks that enemy
   from taking damage (mirrors `test_game_combat.c`'s existing LOS-through-combat
   test style).
3. **No friendly fire.** An allied entity (same team as attacker) standing
   within the blast radius takes no damage, and the attacker itself (if
   adjacent to its own blast) takes no damage.
4. **AP is spent exactly once.** A successful AoE cast spends
   `skill.ap_cost` once regardless of how many entities the blast hits.
5. **Out-of-range/LOS impact tile fails cleanly.** Clicking a tile beyond
   `skill.range`, or LOS-blocked from the attacker, spends no AP and
   damages nothing (`action_try_attack_area` returning `false`, per F1-03).
6. **Multi-kill turn-order reconciliation.** A blast that kills 2+ enemies
   in one cast removes all of them from `game.turn.order`, leaves
   `game.turn.cursor` pointing at a valid (still-alive) entity, and leaves
   every remaining `turn.order` entry alive (mirrors the invariant
   `turn_remove_dead_entity` itself asserts at `turn.c:76-80`, exercised
   here through the public API across multiple casualties).
7. **Multi-kill triggers game over.** A blast that kills every remaining
   enemy (or, if a scenario allows it, every remaining player) sets
   `game.game_over` to the correct value exactly once.
8. **AoE cast onto an empty tile.** Casting with no entity on the impact
   tile itself still works and hits whichever entities are in the
   surrounding radius (validates F1-04's tile-click dispatch path, not just
   the entity-click path).

## Acceptance criteria
- All 8 scenarios above pass as `test_case_t` entries in
  `g_game_aoe_tests`.
- `src/test.c` and `CMakeLists.txt` correctly register/build the new suite.
- Existing suites (`test_game_combat`, `test_game_movement`, others) remain
  unmodified and passing.
- Tests run cleanly under whatever `assert_debug`-enabled build config the
  project's test target already uses (no new assert failures introduced by
  the AoE code paths, including `turn_remove_dead_entity`'s internal
  asserts).

## Testing
This ticket *is* the testing work — see "Scope" above for the concrete
cases. Run via this project's existing test target (same invocation used
for `test_game_combat`/`test_game_movement`).

## Dependencies
- F1-01, F1-02, F1-03, F1-04 (all must be implemented for these tests to
  exercise real behavior).

## Non-goals
- No hover/preview test coverage (F1-08).
- No fuzz-suite integration (`test_game_fuzz.c`) — out of scope unless a
  follow-up ticket adds it explicitly.
