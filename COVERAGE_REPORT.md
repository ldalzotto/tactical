# Coverage work report

## Goal

Reach **0 missing coverage** reported by `npm test` (which now runs the
coverage build + report) across every compiled source file, with no file
filtering in the coverage tool.

## Final state

```text
npm test
Tests: 92/92 passed, 0 failed
Annotated line coverage: build/coverage-html/index.html
```

`server/coverage-missing.js` reports **0 uncovered regions**. The only
intentionally-ignored region is `src/lib/assert.c:16:9` (`__builtin_trap`),
because executing it traps the wasm and fails the test run.

## What changed

### Coverage tool

- Removed the `INCLUDED_FILES` allowlist from `server/coverage-missing.js` and
  the `--ignore-filename-regex` filters from `server/coverage.js`. Coverage now
  reports every compiled source, including `main.c`, `render.c`, `input.c`,
  `clock.c`, `runtime.c`, `graphics.c`, the test harness, and macro expansions.
- `package.json`: `npm test` now runs
  `node server/build.js --tests --coverage && node server/coverage.js`.

### Production code

- `src/lib/clock.c`: removed the unreachable trailing `return 0`.
- `src/game/render.c`: the provably-dead `fg_width` clamps
  (`fg_width < 0`, `fg_width > bar_width`) were replaced with
  `assert_debug(fg_width >= 0)` / `assert_debug(fg_width <= bar_width)`.
- `src/main.c`:
  - Moved `app_state_t` into a new `src/main.h` so tests can call the exported
    entry points.
  - Extracted `app_dispatch_input_events(...)` out of `onNextFrame` so the
    event loop can be tested with a non-empty slice (the JS test runner always
    polls zero events).

### Tests

Added/updated tests (69 -> 92 total):

- `src/test_app.c` (new): calls `init`, `deinit`, `onNextFrame` (both the
  wait-early-return and full-frame paths), and `app_dispatch_input_events` with
  a synthetic click. This covers `main.c`, `clock.c`, `input.c`, `runtime.c`,
  and the `STR` macro expansion in `memory.h`.
- `src/test_runtime.c`: added an out-of-range `test_discovery_*` call to cover
  `test_lookup`'s fallback `assert_test(false)` path.
- `src/test_game_movement.c`: replaced flag-scanning loops with
  `test_tile_list_contains` so the test code itself has no uncovered
  if-bodies.
- `src/test_render.c`: added render tests for obstacle tiles, hover outline,
  small-tile HP-bar/entity-metric clamps, zero-`max_hp` foreground, dead-entity
  skipping, WIN/LOSE game-over screens (including `fb_width == 0`), skill
  buttons with more than two skills, enemy-active HUD button colors, and the
  `test_tile_fully_color` false branch.
- `CMakeLists.txt`: registered `src/test_app.c`.
- `src/test.c`: registered the new app suite.

### Dead/defensive code

- `position_sub` and `entity_is_adjacent` were unused and were removed earlier.
- `action_try_attack`'s dead-entity guard and `ai_step_toward`'s `!found` guard
  were converted to `assert_debug` invariants earlier.

## Remaining ignored region

`src/lib/assert.c:16:9` — `__builtin_trap()` inside `panic`. This is the trap
itself, not a defensive branch that can be rewritten: covering it would require
a failing/trapping test, which would make the coverage runner skip the report.
