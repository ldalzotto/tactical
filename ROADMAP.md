# Tactical — Codebase Investigation Report

**Date:** 2026-08-18
**Scope:** full repository (`src/`, `web/`, `server/`, build system)
**Baseline:** `d3fd870` (master, clean tree)
**Size:** 2,567 lines production C · 3,869 lines test C · 1,317 lines JS

---

## 0. How to read this report

Every finding is tagged:

| Tag | Meaning |
|---|---|
| **CONFIRMED (repro)** | I wrote a temporary test against this tree, built it, and observed the behaviour. The probe was then reverted (`git checkout`) — the tree is clean. |
| **CONFIRMED (code)** | Unambiguous from reading the code; no repro needed. |
| **HYPOTHESIS** | Reasoned from the code, not executed. Stated with its trigger condition so you can decide whether it is worth guarding. |

Scores are `Impact 1–5` / `Effort 1–5` / `Confidence`.

**Baseline health check.** `npm test` → **106/106 pass**, and coverage is *gated*: `server/coverage-missing.js` fails the build on any uncovered line or branch. This codebase has 100% enforced line **and** branch coverage.

That fact is the single most important context in this report. **Every defect below survives 100% branch coverage.** They are all state-machine, metric, and invariant bugs — the lines are executed, just never in the combination that is wrong. Section 7 (Testing) argues this is the highest-leverage place to invest.

---

## 1. Executive summary

The engineering quality here is unusually high for a hobby-scale project: freestanding wasm32 with no libc, an explicit LIFO arena with no hidden allocation, `PUBLIC`/`PRIVATE` linkage discipline that collapses to `static` in unity builds, `-Wall -Wextra -Werror`, a bespoke wasm test runner with expected-trap support, DWARF symbolication of wasm traps back to `file:line`, and coverage-gated CI. The comments are genuinely load-bearing.

The problems are not sloppiness. They are **three structural gaps**, each of which produces several symptoms:

1. **The mode state machine leaks.** `game_on_tile_pressed` gates on `mode == GAME_MODE_NONE` but not on `GAME_MODE_ATTACK`, so a tile click in attack mode falls through to movement. This is your reported bug — confirmed with a repro (**D1**). It is the same missing dispatch layer that blocks tile-targeted skills, i.e. AoE.
2. **Range is measured with the wrong metric.** `pathing.h` documents Manhattan distance; `pathing_bfs` computes walkable-geodesic distance. A target two tiles away in a clear straight line is out of range for a range-3 skill if the tile between them is non-walkable (**D3**, confirmed with a repro). Latent today only because the default scenario always pairs `walkable=false` with `blocks_sight=true`.
3. **The bounds primitive everything rests on is off by one.** `slice_at` asserts `result <= s.end`, so `SLICE_AT(s, count)` — one element past the end — passes the assert and silently overwrites the next allocation (**D2**, confirmed with a repro). This is the *only* guard protecting every framebuffer write, grid index, and entity access, and it is compiled out entirely in Release.

Beyond defects, the architecture is holding **three unused capabilities** that are worth more than most new features: a fully deterministic, I/O-free game core driven by a 12-byte POD event stream (→ free replay, undo, reproducible crash reports, lockstep multiplayer); a `blocks_sight`/`walkable` bit pair whose fourth combination is unused (→ chasms and windows, for one line of scenario code); and a hover tile that is computed every frame and used only to draw an outline.

**The headline architectural recommendation** is in **A1**: the most dangerous machinery in the codebase — `game_scratch_push`, `linear_allocator_insert`, and the `ptrdiff_t shift` value that has to be propagated by hand through `game_set_mode` → `game_on_input_event` → `app_dispatch_input_events` — exists *only* because the range overlay is a variable-length tile list. Caching the **distance field** (`W*H` int32s, fixed size, 640 bytes at 16×10) instead of a position list deletes all of it, is smaller than the worst-case list, and is strictly more informative (it carries per-tile cost, which unlocks path preview and MP readout for free).

---

## 2. Confirmed defects

### D1 — Movement fires while the attack range overlay is displayed
**CONFIRMED (repro)** · Impact **4** · Effort **1** · Confidence **certain**
*(This is the bug you identified. Confirmed exactly as described.)*

**Evidence:** `src/game/game.c:272-293`

```c
PRIVATE ptrdiff_t game_on_tile_pressed(game_state_t *game, linear_allocator_t *allocator, position_t target) {
    ...
    if (game->mode == GAME_MODE_NONE) {
        return 0;
    }
    ...
    if (action_try_move(allocator, game->grid, game->entities, active, target)) {
        return game_set_mode(game, allocator, GAME_MODE_MOVEMENT);
    }
```

The only mode rejected is `NONE`. `GAME_MODE_ATTACK` falls straight through to `action_try_move`.

**Repro (run against this tree, then reverted).** 6×3 grid, player at (0,0) with 3 MP and a melee skill, enemy at (5,2). Select the player, click the attack toggle, then click empty tile (2,0):

```c
assert_test(game.mode == GAME_MODE_ATTACK);
assert_test(!test_tile_list_contains(game.render.attack_range_tiles, (position_t){2, 0}));

test_click_tile(&game, allocator, (position_t){2, 0});

assert_test(p->position.x == 2);          // moved
assert_test(p->mp == 1);                  // spent 2 MP
assert_test(game.mode == GAME_MODE_MOVEMENT);  // silently dropped out of attack mode
```
→ **passes**, i.e. all three wrong behaviours occur.

**Problem.** The player is looking at an orange attack-range overlay. Clicking a tile *outside* that overlay — a tile with no affordance suggesting it is clickable — teleports the unit, spends MP, and silently exits attack mode. `action_try_move` does not consult `render.attack_range_tiles` at all; it re-runs its own movement BFS. There is no undo. In a tactics game, an unintended move is frequently a lost unit.

**Why no test caught it.** `src/test_game_movement.c:9-16` documents the team's own testing philosophy: *"Behavior that the game API structurally prevents a player from ever triggering ... has no equivalent test here: there's no click that reaches it."* The assumption was that attack mode structurally prevents movement. It does not. The line `action_try_move(...)` is 100% covered — just never from `ATTACK`.

**Fix.** Do not patch the `if`; split the dispatch. `game_on_tile_pressed` is doing two jobs under one name.

```c
PRIVATE ptrdiff_t game_on_tile_pressed(game_state_t *game, linear_allocator_t *allocator, position_t target) {
    ...
    if (game->mode == GAME_MODE_MOVEMENT) {
        if (action_try_move(...)) {
            return game_set_mode(game, allocator, GAME_MODE_MOVEMENT);
        }
        return 0;
    }
    if (game->mode == GAME_MODE_ATTACK) {
        return game_on_tile_targeted(game, allocator, target);   // no-op today; AoE lands here (F1)
    }
    return 0;
}
```

Doing it this way makes D1's fix and the AoE feature **the same refactor** — `game_on_tile_targeted` is precisely the hook tile-targeted skills need. Fixing D1 with a one-line guard would have to be undone to build F1.

**Tests required.**
- Attack mode + click empty tile in movement range → position, MP, and mode all unchanged.
- Attack mode + click empty tile *inside* the attack overlay → still no move (until F1 lands).
- Regression: movement mode + click empty tile → still moves (existing `game_tile_pressed_moves_within_reach_and_consumes_mp` covers this).
- Toggle attack → toggle back → move still works.

---

### D2 — `SLICE_AT` bounds assert is off by one; writing one past the end is permitted
**CONFIRMED (repro)** · Impact **5** · Effort **1** · Confidence **certain**

**Evidence:** `src/lib/memory.c:77-84`

```c
PUBLIC void *slice_at(slice_t s, size_t index, size_t alignment) {
    assert_debug((alignment & (alignment - 1)) == 0);
    void *result = byteoffset(s.begin, (ptrdiff_t)index);
    assert_debug(result <= s.end);        // <-- allows result == s.end
    assert_debug(result >= s.begin);
    ...
```

`SLICE_AT(s, N)` on an N-element slice computes `result == s.end`, passes the assert, and returns a pointer to the first byte *after* the slice. `SLICE_AT` is an lvalue macro (`src/lib/memory.h:50-51`), so this is a live write.

**Repro (run against this tree, then reverted).**

```c
a = LINEAR_ALLOCATOR_PUSH(allocator, a, 4);       // 4 int32s
guard = LINEAR_ALLOCATOR_PUSH(allocator, guard, 1);
SLICE_DEREF(guard) = 1111;

expect_panic_begin();
SLICE_AT(a, 4) = 9999;                            // index == count
assert_test(expect_panic_end() == false);         // no panic raised
assert_test(SLICE_DEREF(guard) == 9999);          // adjacent allocation stomped
```
→ **passes**. No panic; the neighbouring allocation is corrupted.

**Problem.** `slice_at` is the single bounds check in the entire codebase. Every framebuffer pixel (`graphics.c:10`), every grid tile (`grid.c:38`), every entity, every pathing cell goes through it. A one-past-the-end write is the classic off-by-one, and this primitive is designed to catch exactly that and does not. There is also an existing test — `slice_at_panics_on_out_of_bounds` — which passes, so the suite reports this area as verified.

Severity is amplified by `assert.h:25-29`: `assert_debug` is `panic` only when `NDEBUG` is undefined. `npm run build:release` defines `NDEBUG`, so **in Release there is no bounds checking at all** — not off-by-one, none. The arena is one contiguous block, so an out-of-range index does not fault; it silently rewrites game state.

**Fix.** `assert_debug(result < s.end);` — and expect fallout. Any loop currently relying on the permissive `<=` (e.g. building an end sentinel via `SLICE_AT`) will start tripping. That fallout is the point: it is enumerating real latent bugs. Note `slice_advance` (`memory.c:86-90`) correctly uses `<=`, because *advancing to* the end is legal; *dereferencing* it is not. The two operations need different bounds, which is why one macro cannot serve both.

**Tests required.**
- `slice_at_panics_on_index_equal_to_count` (the probe above, inverted).
- Keep `slice_at_panics_on_out_of_bounds` for index > count.
- A guard-word test proving no neighbouring write occurs.
- Rebuild `--release` and re-run the full suite to confirm nothing depended on the old bound.

---

### D3 — Skill range is walkable-geodesic distance, not the documented Manhattan distance
**CONFIRMED (repro)** · Impact **4** · Effort **2** · Confidence **certain**

**Evidence:** `src/game/pathing.h:32-44` vs `src/game/pathing.c:16-86`, `113-135`.

The header states:

> *"Range for a skill: tiles within Manhattan distance max_range of `from` that also have a clear line of sight"*
> *"dist is Manhattan distance for tiles with clear LOS, -1 otherwise"*

But `pathing_compute_line_of_sight` broad-phases through `pathing_bfs`, a 4-neighbour flood fill that skips `!grid_is_walkable` tiles (`pathing.c:60-62`). A flood fill that routes around walls yields **geodesic** distance. It equals Manhattan distance only on an obstacle-free grid.

**Repro (run against this tree, then reverted).** 5×3 grid. Tile (1,1) is a *chasm*: `walkable = false`, `blocks_sight = false` — you can see straight across it. Player at (0,1) with `SKILL_RANGED` (range 3). Enemy at (2,1). Manhattan distance is 2. The straight ray (0,1)→(2,1) crosses only (1,1), which does not block sight.

```c
assert_test(!test_tile_list_contains(game.render.attack_range_tiles, (position_t){2, 1}));
test_click_tile(&game, allocator, e->position);
assert_test(e->hp == 10);   // attack refused
assert_test(p->ap == 5);    // no AP spent, no feedback
```
→ **passes**. The enemy is two tiles away in plain sight and cannot be shot with a range-3 weapon.

**Problem.** Range silently contracts around terrain in a way nothing on screen explains. The player sees a clear line and an unlit tile. Two compounding issues:

1. **This is live today, not theoretical.** `grid_init` defaults `blocks_sight = false` (`grid.c:22`), and commit `5603f93` deliberately decoupled the two bits. Any scenario author who writes `grid_set_walkable(p, false)` without also writing `grid_set_blocks_sight(p, true)` hits this immediately. `scenario_setup_default` (`scenario.c:8-13`) carefully pairs them — but that pairing is an undocumented convention, not an invariant.
2. **It blocks feature F4** (chasms / low walls / windows), which is otherwise a zero-code content win.

**Fix.** Decide the intended metric explicitly, then make code and docs agree.

Recommended — **true Manhattan + LOS**, which is what the docs promise and what players expect from a projectile:

```c
PUBLIC bool skill_target_in_range(..., entity_t *attacker, skill_t skill, entity_t *target) {
    int dx = attacker->position.x - target->position.x;
    int dy = attacker->position.y - target->position.y;
    int manhattan = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
    if (manhattan > skill.range) return false;
    return pathing_line_of_sight_clear(grid, entities, attacker->position, target->position);
}
```

`pathing_line_of_sight_clear` already exists (`pathing.c:96-111`), currently `PRIVATE`; promote it. This also fixes **P1** below (a ~160× speedup) since the whole allocator parameter disappears. For the overlay, replace `pathing_bfs`'s broad phase with a direct diamond scan over `|dx|+|dy| <= max_range`, keeping the same raycast narrow phase.

**Tests required.**
- The chasm repro above, with the assertion inverted (attack succeeds, AP spent, HP reduced).
- Range at exactly `max_range` and at `max_range + 1` across open ground.
- LOS still blocked by a sight-blocking wall at the same distance (protects `game_ranged_attack_blocked_by_wall_on_diagonal_line`).
- LOS still blocked by an intervening entity (protects `game_ranged_attack_still_blocked_by_ally_in_path`).
- New: non-walkable + non-sight-blocking tile does **not** reduce range.

---

### D4 — Turn order desynchronises when the active entity dies
**CONFIRMED (repro)** · Impact **4** (once AoE lands; **1** today) · Effort **1** · Confidence **certain**

**Evidence:** `src/game/turn.c:50-71`

```c
PUBLIC turn_state_t turn_remove_dead_entities(turn_state_t state) {
    entity_t *active = turn_active_entity(state);
    slice_entity_ptr_t write = state.order;
    int new_cursor = 0;                        // <-- silent fallback
    for ( SLICE_FOREACH(state.order, read) ) {
        entity_t *entity = SLICE_DEREF(read);
        if ( entity->alive ) {
            if (entity == active) {
                new_cursor = (int)typesize(state.order.begin, write.begin);
            }
            ...
```

If `active` is itself dead it is never re-encountered in the compaction, so `new_cursor` keeps its initialiser `0`.

**Repro (run against this tree, then reverted).** Order `[a, b, c]`, cursor advanced to `b`, then `b` dies:

```c
entity_damage(b, 100);
turn = turn_remove_dead_entities(turn);

assert_test(turn.cursor == 0);
assert_test(turn_active_entity(turn) == a);   // a is "active" again, out of order
turn = turn_advance(turn);
assert_test(turn_active_entity(turn) == c);   // c acts; b's slot is simply gone
```
→ **passes**. The cursor snaps to the head of the order.

**Problem.** Today this is **unreachable**: nothing can damage the active entity. `action_try_attack` rejects same-team targets (`action.c:31-33`), and the only caller passes the active entity as attacker. So current impact is 1.

**The moment you add area-of-effect damage, it becomes reachable** — a blast centred near the caster, or a reflected/hazard tile, kills the caster. Then the turn order silently rewinds to entity 0 and one unit gets a free extra turn while another is skipped. Because there is no assert and no test, it will present as "the turn order sometimes goes weird after a big explosion", which is a miserable bug to chase.

This is the clearest example of the report's theme: **the feature you want (AoE) activates a dormant bug.** Fix D4 *before* F1, not after.

**Fix.** Make the intent explicit rather than defaulting:

```c
PUBLIC turn_state_t turn_remove_dead_entities(turn_state_t state) {
    entity_t *active = turn_active_entity(state);
    slice_entity_ptr_t write = state.order;
    // If the active entity died, the next survivor at or after its slot
    // becomes the cursor, so turn_advance still moves to the correct
    // successor instead of rewinding to the head of the order.
    int active_index = state.cursor;
    int new_cursor = -1;
    ...
```

Then after compaction: if `new_cursor < 0`, set it to the number of survivors that were *before* `active_index`, minus one, so the following `turn_advance` lands on the right successor (with wraparound). Add `assert_debug(SLICE_TYPESIZE(state.order) > 0)` — see H1.

**Tests required.**
- Active entity dies mid-order → next entity in the original order acts next.
- Active entity is last in order and dies → wraps to index 0.
- Active entity dies **and** another entity dies in the same call.
- Existing `game_turn_order_compacts_when_non_active_entity_dies_during_attack` must still pass.

---

### D5 — Panic reporting infrastructure is fully built and completely unwired
**CONFIRMED (code)** · Impact **3** · Effort **2** · Confidence **certain**

**Evidence:** `grep -rn "report_panic" src/ web/ server/` returns exactly one hit — `web/wasm-shared.js:72`. Nothing in C imports it.

Three pieces exist and are wired to each other, but not to the C side:
- `web/wasm-shared.js:72-79` — a `report_panic(filePtr, fileLen, line, msgPtr, msgLen)` import that decodes a file name, a line number and a message from wasm memory.
- `web/main.js:136-138` — a `reportPanic` callback that renders a formatted `PANIC\n\n{file}:{line}\n\n{message}` overlay.
- `src/lib/assert.c:10-21` — `panic(bool)` which takes **no message and no location** and calls `__builtin_trap()`.

**Problem.** Every assertion failure in this codebase — and `assert_debug` is used ~40 times as the primary correctness mechanism — surfaces as an anonymous `RuntimeError: unreachable`. The fallback path (`main.js:58-63`) then does a network round-trip to `/__symbolicate`, spawns `llvm-symbolizer`, and parses DWARF just to recover a `file:line` that the C compiler knew at compile time. When symbolication fails (no `llvm-symbolizer` on PATH, or a Release build with stripped debug info), the developer gets `unreachable` and nothing else.

Meanwhile the direct, zero-dependency path is already written and receiving no calls.

**Fix.** Give `panic` a location, keeping the existing signature working:

```c
// assert.h
PUBLIC void panic_at(bool condition, const char *file, int line, const char *msg);
#define panic(cond) panic_at((cond), __FILE__, __LINE__, #cond)
```

`panic_at` calls the `report_panic` import before trapping (guarded by `expect_panic`/`expect_trap` exactly as today, so the test harness is unaffected). The stringified condition `#cond` gives you the assertion text for free. Failures become `PANIC src/game/pathing.c:138: grid_in_bounds(grid, position)` with no symbolication step.

**Tests required.**
- `expect_panic_begin/end` still swallows a failing `panic` (existing tests must pass unchanged).
- A test asserting `report_panic` receives the right file/line — needs a JS-side spy in `buildImportObject`; `run-tests.js` can assert on it.
- Trap path unchanged (`panic_without_expect_panic_traps`).

---

### D6 — `test_run` hands each test an arena that extends past the end of wasm memory
**CONFIRMED (code)** · Impact **2** · Effort **1** · Confidence **high**

**Evidence:** `src/test.c:82-93` and `web/wasm-shared.js:124`

```c
void test_run(test_fn_t fn, uint32_t memory_size) {
    ...
    slice_t data = { heap_base(), byteoffset(heap_base(), (ptrdiff_t)memory_size) };
```
called as `test_run(fn, memory.buffer.byteLength)`.

`heap_base()` returns `&__heap_base`, an offset *into* linear memory (after static data and stack). Adding the **full** buffer length to it produces an `data.end` that is `__heap_base` bytes past the end of linear memory.

**Problem.** `linear_allocator_push`'s `assert_debug(end <= allocator->data.end)` (`memory.c:23`) is the arena's overflow guard, and in tests it is calibrated to the wrong ceiling. A test that allocates close to the limit — plausible for `game_attack_toggle_with_large_range_skill_grows_scratch`, which deliberately grows the arena — passes the assert and then writes past linear memory. Wasm traps on the out-of-bounds store, so it is memory-safe, but it surfaces as an unexplained `RuntimeError` instead of the clean arena-overflow panic the assert exists to give. Also, `linear_allocator_push_panics_on_overflow` currently verifies the guard against a fabricated small arena, not this one.

**Fix.** `test_run(fn, memory_size)` should subtract the heap base:

```c
uintptr_t base = (uintptr_t)heap_base();
slice_t data = { heap_base(), byteoffset(heap_base(), (ptrdiff_t)(memory_size - base)) };
```

**Tests required.** A test that pushes `memory_size` bytes and asserts a clean panic rather than a trap.

---

## 3. Hypotheses and latent risks

### H1 — `turn_active_entity` on an empty turn order reads out of bounds
**HYPOTHESIS** · Impact **4** · Effort **1** · Confidence **medium**

`turn_remove_dead_entities` (`turn.c:66`) can produce `state.order.end == state.order.begin`. `turn_active_entity` (`turn.c:38-40`) is `SLICE_AT(state.order, state.cursor)` with no emptiness check, and `turn_advance` asserts `count > 0` (`turn.c:44`) — only in debug.

Currently unreachable because a team must survive for the game to continue, and `game_check_game_over` fires first. But note `game_advance_turn` (`game.c:224-236`) calls `turn_advance` **unconditionally after** the game-over check inside the loop body:

```c
while (game->game_over == GAME_OVER_NONE && active->team == ENTITY_TEAM_ENEMY) {
    ... game_check_game_over(game);
    game->turn = turn_advance(game->turn);      // runs even when the game just ended
    active = turn_active_entity(game->turn);
}
```

so the ordering already relies on a non-obvious invariant. Mutual destruction (AoE again) or a hazard that kills the last two entities simultaneously would empty the order.

**Fix.** `assert_debug(SLICE_TYPESIZE(state.order) > 0)` in `turn_active_entity`, and hoist the game-over check so `turn_advance` is skipped once the game ends.

---

### H2 — AI dereferences a null skill pointer for a skill-less entity
**HYPOTHESIS** · Impact **3** · Effort **1** · Confidence **high**

**Evidence:** `src/game/ai.c:115-124, 153-157`

```c
PRIVATE skill_t* ai_preferred_skill(entity_t *enemy) {
    skill_t *best = 0;
    for (SLICE_FOREACH(enemy->skills, skill_s)) { ... }
    return best;                              // 0 when skills is empty
}
...
skill_t *preferred = ai_preferred_skill(enemy);
while (enemy->mp > 0 && !skill_target_in_range(..., *preferred, target)) {   // deref
```

`entity_spawn` creates entities with `.skills = {0}` (`entity.c:32`); the scenario assigns skills afterwards. An enemy that is spawned but not given skills — a summon, a civilian, a scenario typo — reaches this line.

**Why it is worse than a crash.** In wasm32, address 0 is ordinary readable linear memory, not a trap page. Dereferencing null yields `skill_t{0,0,0}` from the module's zero-filled low memory. Result: range 0, so `skill_target_in_range` never succeeds, so the AI burns its entire MP walking toward the target every turn and then attacks with nothing. A silently pacifist unit that jitters across the map — a *behaviour* bug, not a crash, and therefore very hard to trace back to a missing skill assignment.

**Fix.** `if (preferred == 0) return 0;` immediately after the call. Add `assert_debug(entity_skill_count(enemy) > 0)` if skill-less entities are meant to be impossible — pick one and state it.

---

### H3 — `ai_step_toward` can fail to move, producing an infinite loop
**HYPOTHESIS** · Impact **5** if triggered · Effort **1** · Confidence **low–medium**

**Evidence:** `src/game/ai.c:64-101, 155-157`

```c
while (enemy->mp > 0 && !skill_target_in_range(...)) {
    ai_step_toward(allocator, grid, entities, enemy, target);
}
```

The loop's only termination guarantee is that `ai_step_toward` decrements `mp`. But `ai_step_toward` ends with an **unchecked** `action_try_move(...)` (`ai.c:100`), and `action_try_move` returns `false` without mutating anything when the destination is unreachable (`action.c:14-16`). Two paths reach that state:

1. `best_position` is chosen from a BFS rooted at the **target**, in which the target's own tile has distance 0. If every free neighbour of the enemy is occupied except the direction of the target itself, `best_position` is the target's occupied tile — and the enemy's own BFS (which blocks on entities) refuses to move there.
2. `assert_debug(found)` at `ai.c:99` is compiled out in Release; if no neighbour is reachable, `best_position` stays `{0,0}` and the move to (0,0) almost certainly fails.

I could not construct a reachable trigger on the current rule set — an adjacent target always satisfies range ≥ 1 with a clear (empty) ray, so the loop does not run. But that safety comes from an incidental interaction between three modules, not from any stated invariant.

**Consequence if triggered:** `app_on_next_frame` never returns. The browser tab hangs with no error, no trap, no overlay. This is the worst failure mode available in this architecture, and it costs one line to make impossible.

**Fix.** Make the step report progress and make the loop respect it:

```c
PRIVATE bool ai_step_toward(...) { ... return action_try_move(...); }
...
while (enemy->mp > 0 && !skill_target_in_range(...)) {
    if (!ai_step_toward(allocator, grid, entities, enemy, target)) break;
}
```

Belt and braces: bound the loop by the initial MP.

**Tests required.** Enemy boxed in on three sides by allies with the target on the fourth; enemy with MP but zero reachable neighbours. Both must return within one turn.

---

### H4 — `graphics.c` has no clipping; Release has no bounds checks either
**HYPOTHESIS** (for a specific trigger) / **CONFIRMED (code)** for the absence of clipping · Impact **4** · Effort **2** · Confidence **high**

**Evidence:** `src/lib/graphics.c:7-13`

```c
PUBLIC void graphics_draw_rectangle(slice_rgba_t framebuffer, int fb_width, int x, int y, int width, int height, rgba_t color) {
    for (int j = 0; j < height; j++)
        for (int i = 0; i < width; i++)
            SLICE_AT(framebuffer, (y + j) * fb_width + (x + i)) = color;
```

No clamping of `x`, `y`, `width`, `height` against the framebuffer. The sole guard is `SLICE_AT` → `slice_at` → `assert_debug`, which (a) is off by one (**D2**) and (b) **does not exist in Release**.

Negative `x` or `y` computes a negative index; `byteoffset` accepts it; in Release nothing checks it.

**Reachable triggers today** are unbounded loops in `render.c` that draw from game data with no cap:
- `render_hud:250-258` — one AP pip per point, `for (i = 0; i < selected->ap; i++)`. No clamp. A high-AP unit walks the pip row off the right edge and, since the row sits at the bottom of the framebuffer, eventually past its end (~AP 280 at 320×240).
- `render_timeline:184-195` — one square per entity in the turn order, no clamp, `x = area.x + i * (square + gap)`.

Neither is reachable with the current 1-AP, 6-entity scenario. Both become reachable with *any* content change — a boss with 8 AP, a 12-unit skirmish.

**Fix.** Clip once, in the primitive, where it is correct for all callers:

```c
PRIVATE void graphics_clip(slice_rgba_t fb, int fb_width, int *x, int *y, int *w, int *h) { ... }
```
computing `fb_height` from `SLICE_TYPESIZE(fb) / fb_width` and intersecting the rect, then returning early on an empty result. This is ~12 lines and removes a whole class of Release-only memory corruption. Separately, clamp the pip and timeline loops to what the HUD can actually show, and render an overflow indicator.

**Tests required.**
- Draw a rect straddling each of the four edges; assert in-bounds pixels are painted and nothing outside the framebuffer changes (guard allocations before and after).
- Fully-off-screen rect → no writes.
- Negative width/height → no writes.
- Entity with AP > pip capacity → HUD stays inside `hud_rect`.

---

### H5 — `poll_input_events` writes an unbounded number of events into the arena
**CONFIRMED (code)** · Impact **3** · Effort **2** · Confidence **high**

**Evidence:** `src/game/input.c:3-9` and `web/wasm-shared.js:86-101`

```c
PUBLIC slice_input_event_t input_poll(linear_allocator_t* allocator, window_handle_t window) {
    slice_input_event_t events = LINEAR_ALLOCATOR_PUSH(allocator, events, 0);   // zero bytes
    events.end = poll_input_events(window, events.begin);
    // We simulate the js side to have allocated some memory to the allocator
    allocator->cursor = events.end;
```
```js
poll_input_events(windowHandle, beginPtr) {
    const events = pendingInputEvents.get(windowHandle) ?? [];
    const writeCount = events.length;                     // no cap
    const view = new DataView(memory.buffer);
    for (let i = 0; i < writeCount; i++) { ...setInt32... }
```

C pushes a **zero-length** slice, JS writes `writeCount * 12` bytes starting at the cursor, and C then retroactively moves the cursor. The allocator's `assert_debug(end <= data.end)` is bypassed entirely — the write happens before any push, so the guard never sees the size.

**Problem.** The arena's overflow protection has a hole exactly at the one boundary where untrusted-ish, unbounded external data enters. `mousemove` fires at hundreds of Hz; the queue drains only when `app_on_next_frame` reaches its 16 ms gate (`app.c:73-77`). If the main thread stalls, the queue grows without bound. The failure mode is silent arena corruption up until the point `DataView.setInt32` finally throws a `RangeError` at the memory boundary — by which time game state above the cursor is already overwritten.

**Fix (two-sided, both cheap).**
1. C: pass a capacity. Push a fixed `INPUT_EVENT_MAX` slice, hand JS both `begin` and `end`, then shrink to what JS returned via `linear_allocator_pop`. This puts the write back under the allocator's own bound.
2. JS: `const writeCount = Math.min(events.length, capacity)` and keep the remainder queued (the `events.slice(writeCount)` line already implements exactly this drain semantics — it is just never exercised because `writeCount` is always `events.length`).
3. **Coalesce mouse moves.** Only the last `mousemove` per frame can affect `game->hover`. Collapsing consecutive moves is a few lines and removes the pathological case at the source.

**Tests required.** Queue more events than capacity; assert the arena bound holds, the first `capacity` events are dispatched, and the remainder arrive on the next poll. `test_push_input_event` (`wasm-shared.js:83-85`) already exists to drive this.

---

### H6 — Dev server binds all interfaces and exposes a subprocess-spawning endpoint
**CONFIRMED (code)** · Impact **2** (dev-only) · Effort **1** · Confidence **high**

**Evidence:** `server/server.js:128` — `server.listen(PORT, ...)` with no host argument binds `0.0.0.0`. `server.js:88-91` routes `POST /__symbolicate` to a handler that spawns `wasm-objdump` and `llvm-symbolizer` (`symbolicate.js`). There is no `Origin`/`Host` check and no CORS policy.

The path traversal defence is **sound** — `resolveFile` (`server.js:38-47`) joins then verifies `filePath.startsWith(route.dir + path.sep)`, which correctly rejects `/build/../src/app.c`. Arguments are passed via `spawn` with an array, so there is no shell injection. The residual risks are:

- Anyone on the LAN can read everything under `web/` and `build/`, including the wasm with full DWARF debug info (source paths, absolute filesystem layout, complete symbol names).
- Any website the developer visits can `fetch('http://localhost:8081/__symbolicate', {method:'POST', ...})` and spawn a pair of LLVM processes per request — trivial CPU exhaustion, and the JSON error path (`server.js:71-74`) echoes `err.message`, which leaks absolute paths.

**Fix.** `server.listen(PORT, '127.0.0.1')`; reject `/__symbolicate` unless `Host` is `localhost`/`127.0.0.1`; validate that `frames` is an array of `{funcIndex:number, offset:string}` with a length cap before spawning anything.

---

## 4. Architecture and technical debt

### A1 — ★ Cache the distance *field*, not a tile *list* — this deletes the arena-growth machinery
**Impact 5 · Effort 3 · Confidence high**

**This is the highest-leverage change in the report.**

**Evidence.** The single most complex mechanism in the codebase exists solely to support a variable-length overlay:

| Location | What it does |
|---|---|
| `game.c:88-120` | `game_scratch_push` — grows `game->scratch` in place, copies the staged list in, rebases `pathing` and `temp` |
| `memory.c:53-67` | `linear_allocator_insert` — opens a gap mid-arena by `memmove`-ing everything above it |
| `game.c:137-213` | `game_set_mode` returns a `ptrdiff_t shift` |
| `game.c:241-397` | every `game_on_*_pressed` propagates that shift as its return value |
| `app.c:56-69` | `app_dispatch_input_events` rebases the in-flight event pointer *and* `events.end` mid-loop |
| `app.c:83-86` | `app_on_next_frame` rebases `events` again before popping |
| `render_cache.c:8-11` | `render_cache_assert_layout` guards a stacking invariant that only exists because the regions are variable-sized |

That is roughly 80 lines of manual pointer rebasing whose failure mode is a dangling pointer into a `memmove`d arena. Commits `835dc07`, `b213589`, `5725cb4`, `b8089d8`, `4a7336d`, `e9f9ad4` are all iterations on getting it right — the git history *is* the evidence of its cost.

**The observation:** all of it exists because `reachable_tiles` / `attack_range_tiles` are lists whose length depends on the skill's range.

But `pathing_state_t` (`pathing.h:15-18`) already computes a **fixed-size** `dist` field of exactly `W*H` `int32`s — and `game_set_mode` (`game.c:157-166`, `190-199`) *throws it away*, iterating the whole grid to filter it down into a position list, which it then has to grow the arena to store.

At 16×10 that field is **640 bytes, always** — smaller than the worst-case position list (160 × 8 = 1,280 bytes) and, critically, **allocated once at `game_init` and never resized**.

**What this buys:**
- Delete `game_scratch_push`, the `shift` return value from six functions, the rebasing in `app_dispatch_input_events` and `app_on_next_frame`, and `render_cache_assert_layout`'s stacking invariant. `linear_allocator_insert` loses its only production caller.
- `render_tiles` iterates the grid and tests `dist[i] > 0` instead of walking a list — same cost, one less indirection.
- **Strictly more information for fewer bytes.** The field carries *per-tile cost*, which the list discards. That single fact unlocks F3 (path preview + MP cost on hover), F5 (threat overlay), and smarter AI scoring, with no additional storage.
- Adding a third overlay (the AoE preview, F1) becomes "another `W*H` field", not "another variable-length region in a hand-maintained stack".

**Implementation.**
1. `render_cache_t` becomes two or three fixed `slice_int32_t` fields plus an enum for which is live, allocated once in `game_init`.
2. `game_set_mode` writes into the pre-allocated field instead of staging and copying (`pathing_bfs` can write directly into it).
3. Delete the shift plumbing; `game_set_mode` and the `game_on_*` handlers return `void`.
4. `render_tiles` reads the field.

**Tests required.** The whole existing suite is the regression test — the observable behaviour must not change. Add: overlay correctness after mode toggles; the arena cursor returns to its pre-selection watermark; `game_attack_toggle_with_large_range_skill_grows_scratch` becomes "large range does not grow the arena at all". Keep `linear_allocator_insert`'s unit tests even after its production caller is gone.

---

### A2 — `game.c` is a monolith mixing input hit-testing, state machine, and orchestration
**Impact 3 · Effort 3 · Confidence high**

`game_on_input_event` (`game.c:341-397`) does screen-space hit testing (`point_in_rect` against three widget kinds), *duplicating the HUD's layout logic including its clamp*:

```c
// game.c:358-363                      // render.c:224-230
if (... entity_skill_count(...) > 1) { if (... entity_skill_count(active) > 1) {
    int button_count = ...;                int button_count = ...;
    if (button_count > VIEWPORT_MAX_SKILL_BUTTONS) {   if (button_count > VIEWPORT_MAX_SKILL_BUTTONS) {
```

Both files carry comments telling the reader to keep them in sync. Two copies of a visibility rule in two modules is a defect waiting to happen — the next widget will be drawn but not clickable, or clickable but invisible.

**Fix.** A single `layout_hit_test(viewport, game_ui_state, x, y) -> ui_target_t` in `layout.c`, consumed by both `game_on_input_event` and `render_hud`. Visibility is then defined once.

---

### A3 — `VIEWPORT_MAX_SKILL_BUTTONS = 2` silently makes skills unreachable
**Impact 3 · Effort 2 · Confidence certain**

`layout.h:11` caps the HUD at two skill buttons. `entity_t.skills` has no cap. An entity with three skills has a third that can never be selected — and the existing test `game_skill_button_hit_test_clamps_more_than_two_skills` *asserts this as intended*, which locks it in.

This is a hard ceiling on every content idea in section 6 (AoE, knockback, status effects all want to be additional skills). The fix is not a bigger constant — it is a skill bar that sizes to the entity, plus keyboard selection (`1`..`9`), which the input layer does not yet support at all (`runtime.h:10-13` has only `MOUSE_MOVE` and `MOUSE_CLICK`).

---

### A4 — `entity_spawn` / `skill_list_add` / `turn_order_add` require append-in-place
**Impact 3 · Effort 3 · Confidence certain**

All three assert `allocator->cursor == list->end` (`entity.c:17`, `entity.c:56`, `turn.c:18`) — an entity can only be created while its list is the top of the arena. `entity.c:16` says it outright: *"We are allowed to push an entity only at the same time where the list is created. For now."*

Consequence: **no entity can ever be created after setup.** No summons, no reinforcements, no spawned hazards, no split units. Several of section 6's ideas need this, and the "For now" is the only TODO in the entire repository.

**Fix.** Reserve capacity at init (`slice_entity_t` with `count` and `capacity`, mirroring `turn_state_t`, which already carries a separate `capacity` field — `turn.h:12`). The pattern exists; it just was not applied to the entity list.

---

### A5 — Scenarios are compiled C, not data
**Impact 3 · Effort 3 · Confidence certain**

`scenario_setup_default` (`scenario.c`) is 73 lines of imperative setup for one map, and the per-entity skill assignment is copy-pasted six times (`scenario.c:31-59`). Every new map is a recompile and a new exported function.

**Fix.** A `scenario_desc_t` POD — grid dimensions, a tile string, an entity table, a turn order — interpreted by one loader. Scenarios then become static const tables (still compiled in, no parser needed, still freestanding), and later a fetched byte blob. Prerequisite for F8 (replay) to name which scenario a recording belongs to, and for any level-editor work.

---

### A6 — The `Wall` / `Grass` / `Chasm` tile convention is undocumented
**Impact 2 · Effort 1 · Confidence certain**

`tile_t` (`grid.c:5-8`) has two independent bools. `scenario_setup_default` establishes by convention that walls set both and tall grass sets only `blocks_sight`, but nothing names or enforces this. The fourth combination is unused — and, per **D3**, currently broken.

**Fix.** Name the archetypes (`TILE_FLOOR`, `TILE_WALL`, `TILE_GRASS`, `TILE_CHASM`) as a helper over the same two bits — no representation change, no cost, and it turns an implicit convention into readable scenario code. See **F4**.

---

## 5. Performance

The game is 16×10 with 6 entities; nothing here is user-visible *today*. Each item is included because it changes the **asymptotics** and therefore caps how far content can scale.

### P1 — ★ `skill_target_in_range` computes an entire range field to answer one yes/no question
**Impact 4 · Effort 1 · Confidence high** — *best effort-to-payoff ratio in the codebase*

`skill.c:10-21` calls `pathing_compute_line_of_sight`, which (`pathing.c:113-135`) runs a full BFS over `W*H` tiles **and then raycasts every tile in range** — each raycast walking up to `range` tiles, each step calling `entity_find_at`, itself an O(N) linear scan (`entity.c:66-75`).

Total: **O(W·H·range·N)** to determine whether one specific target is reachable. The direct answer is `O(range·N)`.

Callers make this hot: `ai_run_ennemy_turn` (`ai.c:155`) calls it once per movement step, and `ai_best_in_range_skill` (`ai.c:128-140`) calls it once per skill. A single enemy turn with 3 MP and 2 skills performs ~5 full-grid computations.

**Fix** is in **D3** — the corrected implementation *is* the optimisation. On a 16×10 grid this is roughly a **160× reduction**, with less code. (It also changes behaviour in the corner case D3 describes, which is the point.)

### P2 — `entity_find_at` is an O(N) scan called from inside every BFS neighbour loop
**Impact 3 · Effort 2 · Confidence high**

`pathing.c:70` calls `entity_find_at` for every neighbour of every dequeued tile: **O(W·H·4·N)** per flood fill. Fine at N=6; quadratic in unit count.

**Fix.** Build an occupancy bitmap (`W*H` bytes) once per `pathing_bfs` call — the arena makes this trivial — turning the inner test into an O(1) array read. Same idea applies to `pathing_line_of_sight_clear` (`pathing.c:105`).

### P3 — Full repaint every frame, 60 fps, for a turn-based game
**Impact 2 · Effort 3 · Confidence high**

`render_frame` (`render.c:261-273`) redraws all 160 tiles, all entities and the whole HUD every frame, then `present_window` copies 320×240×4 = 300 KB across the wasm/JS boundary (`main.js:128-132`). For a game whose state changes only on click, that is ~18 MB/s of memcpy to display a static image.

**Fix.** A `dirty` flag on `game_state_t`, set by `game_on_input_event` when anything changed; skip render and present when clean. Frees essentially the entire frame budget for animation later.

### P4 — `setTimeout` frame loop instead of `requestAnimationFrame`
**Impact 2 · Effort 1 · Confidence certain**

`wasm-shared.js:156-162` drives frames with `setTimeout(tick, waitMs)`, and `clock_time_to_wait` returns 0 whenever a frame is due — so the common path is `setTimeout(0)`, clamped by the browser to ~4 ms and never aligned to vsync. `rAF` gives correct pacing, automatic throttling in background tabs (which also mitigates **H5**), and less jitter. `clock_time_to_wait` stays useful as the fixed-timestep gate.

---

## 6. Hidden and underused capabilities

These are things the codebase **already has** and does not use. Each is the input to a feature in section 7.

### C1 — ★ The game core is a pure, deterministic function of a 12-byte event stream
Nothing in `src/game/` performs I/O, reads a clock, or uses randomness. `input_event_t` is `{int32 type, int32 x, int32 y}` (`runtime.h:15-19`). The entire game state lives in one contiguous arena. Therefore:

- **A match is fully described by its input log.** ~12 bytes per click. A 200-action match is 2.4 KB.
- **Undo is `linear_allocator_copy`** (`memory.c:71-75`) of the arena into a ring buffer — the primitive already exists and is already unit-tested.
- **Any crash is exactly reproducible** from its event log — and `main.js` already renders a trap overlay *with a Copy button* (`main.js:27-38`).
- **Lockstep multiplayer is nearly free** — exchange 12-byte events, not game state.

Nothing uses any of this. See **F8**.

### C2 — Two independent tile bits, only three of four combinations used
`walkable` × `blocks_sight` (`grid.c:5-8`). Used: floor (T,F), wall (F,T), tall grass (T,T). **Unused: (F,F) — see-through but impassable.** Chasms, low walls, windows, water, lava. Zero engine work (once **D3** is fixed). See **F4**.

### C3 — Hover is tracked every frame and used for one white outline
`game->hover` / `hover_valid` are updated on every mouse move (`game.c:388-394`) and consumed once, at `render.c:109-114`, to draw an outline. Every modern tactics UI hangs its entire information layer off the hover tile: path preview, MP cost, hit prediction, blast footprint, unit inspection. See **F1**, **F3**.

### C4 — `pathing_compute_distances` is a general flood fill, rooted anywhere
It takes an arbitrary origin and cap (`pathing.h:30`). `ai_step_toward` already exploits this by rooting the BFS at the *target* rather than the mover (`ai.c:68`) — a nice trick that appears exactly once. Rooting it at every enemy gives you the threat map (**F5**); rooting it at an impact tile gives you a blast footprint that respects walls (**F1**).

### C5 — `graphics_draw_rectangle_dithered` is a general translucency primitive
Written to keep the attack overlay visible under an opaque sprite (`render.c:97-106`), used in exactly one place. It is the natural rendering for every *predictive* overlay — blast preview, threat zone, ghost of a pending move — visually distinguishing "what will happen" from "what is".

### C6 — `ai_skill_beats` is a pluggable scoring predicate
`ai.c:105-110` is a clean comparator, already parameterising two different searches (preferred vs. best-in-range). Generalising it from `skill_t` to a scored *action* (move + skill + target) turns the hardcoded "always charge the nearest player" into swappable AI archetypes. See **F10**.

### C7 — `turn_state_t` already separates `order` from `capacity`
`turn.h:11-13` carries both, so the order can shrink without losing its allocation. That is precisely the shape an *initiative* system needs — the storage for a dynamic turn queue already exists. See **F9**.

### C8 — The test harness can drive real input events through the real event loop
`test_push_input_event` (`wasm-shared.js:83-85`) exists specifically so tests can feed `app_on_next_frame` a non-empty batch. Combined with C1's determinism, this is a ready-made **fuzzing harness** — see section 7.

---

## 7. Testing strategy — the most important recommendation

**The current state:** 106 tests, 100% line and branch coverage, gated in CI. That is genuinely excellent, and it is why the coverage gate must not be blamed here.

**The problem:** every defect in section 2 lives in fully-covered code.

- **D1**: `action_try_move` is covered — never from `ATTACK` mode.
- **D2**: `slice_at`'s bounds assert is covered — by a test asserting the *wrong* bound.
- **D3**: `pathing_bfs` is covered — never with a non-walkable, non-sight-blocking tile.
- **D4**: `turn_remove_dead_entities` is covered — never with a dead *active* entity.

Line coverage measures *"was this line executed"*. Every one of these is *"was this line executed in this state"*. The suite is at the ceiling of what coverage-driven testing can find; further coverage work has near-zero marginal value.

### Recommendation T1 — Invariant (property) testing · Impact 5 · Effort 2
Add `test_invariants.c` asserting properties that must hold after **every** input event, and call it from a wrapper around `game_on_input_event`:

```c
PRIVATE void assert_game_invariants(game_state_t *game) {
    // turn order and the entity list agree about who is alive
    for (SLICE_FOREACH(game->turn.order, e_s)) assert_test(SLICE_DEREF(e_s)->alive);
    assert_test((int)SLICE_TYPESIZE(game->turn.order)
        == entity_alive_count(game->entities, ENTITY_TEAM_PLAYER)
         + entity_alive_count(game->entities, ENTITY_TEAM_ENEMY));
    // no two live entities share a tile; everyone is in bounds; points in range
    // exactly one overlay is populated, and it matches game->mode
    // game_over is consistent with the alive counts
}
```

The overlay/mode invariant alone catches **D1** — it leaves `mode == MOVEMENT` with an overlay computed for a different state. `entity_alive_count` (`entity.c:85-95`) already exists and is currently used only for win/lose.

### Recommendation T2 — Deterministic fuzz driver · Impact 5 · Effort 2
Because of **C1** and **C8**, this is ~40 lines:

```c
PRIVATE void test_fuzz_random_clicks(linear_allocator_t *allocator) {
    game_state_t game = scenario_setup_default(allocator, ...);
    uint32_t seed = 0x9E3779B9;                       // xorshift, no libc needed
    for (int i = 0; i < 20000; i++) {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        input_event_t ev = { .type = (int32_t)(seed & 1),
                             .x = (int32_t)((seed >> 1) % 320),
                             .y = (int32_t)((seed >> 9) % 240) };
        game_on_input_event(&game, allocator, ev);
        assert_game_invariants(&game);
        if (game.game_over != GAME_OVER_NONE) break;
    }
    game_deinit(allocator, game);
}
```

Deterministic (fixed seed → reproducible failure), fast, no dependencies, and it directly exercises the *state combinations* coverage cannot reach. Run several seeds. When it fails, the seed **is** the repro — and per C1, so is the event log.

### Recommendation T3 — Golden-image render tests · Impact 3 · Effort 2
`render_frame` writes into a plain arena buffer and `test_render.c` already scans pixels by colour. Hash the framebuffer (FNV-1a, 10 lines) and pin one hash per scenario state. One assertion catches every unintended visual regression; the existing per-colour scans stay as the readable documentation of *why*.

### Recommendation T4 — Run the suite in Release too · Impact 4 · Effort 1
Every `assert_debug` vanishes under `NDEBUG`, so the Debug suite validates a *different program* from the one `npm run build:release` ships. Given **D2** and **H4**, Release is exactly where bounds violations become silent corruption. Add a `--release` pass to `npm test`. The invariants in T1 use `assert_test` (which is unconditional — `assert.h:31-33`), so they keep working there.

---

## 8. Feature proposals

Format: `existing capability A + existing capability B → new feature`.

---

### F1 — ★ Area-of-effect skills with a live, cover-aware blast preview
*(Your idea, plus the non-obvious part.)*
**`pathing_compute_line_of_sight` (C4) + `render_cache` + hover (C3) → AoE that respects cover, previewed under the cursor**

**Impact 5 · Effort 3 · Confidence high**

**The non-obvious insight:** you do not need new geometry code. A blast footprint is *exactly* a line-of-sight query rooted at the impact tile:

```c
pathing_state_t blast = pathing_compute_line_of_sight(allocator, grid, no_entities, impact_tile, skill.aoe_radius);
```

Because `pathing_compute_line_of_sight` already raycasts from its origin and stops at sight-blocking tiles (`pathing.c:96-135`), an explosion **automatically** fails to bend around a wall corner and **automatically** is stopped by tall grass. You get physically sensible blasts, and the very cover system that makes ranged combat interesting now shapes explosions too — for zero new maths.

**User value.** AoE is the single biggest source of tactical depth in the genre: it rewards positioning, punishes clustering, and creates real decisions (hit two enemies and clip your own tank, or hit one cleanly?). It also gives the AI something to fear, which makes *its* positioning legible.

**Implementation.**
1. `skill_t` gains `int aoe_radius` (0 = single target) and `bool friendly_fire`. It is currently `{range, damage, ap_cost}` (`entity.h:16-20`) — a plain POD, trivially extended, and `SKILL_MELEE`/`SKILL_RANGED` keep working with a `0` default.
2. **Prerequisite: D1's dispatch split.** An AoE skill targets a *tile*, so it lands in `game_on_tile_targeted`. D1's fix creates exactly that hook. **This is why D1 and F1 are one change, not two.**
3. `action_try_attack_area(allocator, grid, entities, attacker, skill, impact)` — validate the impact tile is in `skill.range` with LOS (reusing D3's corrected check), compute the blast field, then apply `entity_damage` to every live entity on a lit tile, honouring `friendly_fire`.
4. **Prerequisite: D4.** With friendly fire the caster can die. Fix the turn cursor first.
5. Preview: on `INPUT_EVENT_MOUSE_MOVE` in attack mode with an AoE skill selected, compute the blast for `game->hover` into a third overlay field (trivial under **A1**; awkward before it) and render it with `graphics_draw_rectangle_dithered` (**C5**) so "what would happen" reads differently from "where I can reach".

**Design note.** Consider damage falloff by distance from impact — the blast field already stores the distance per tile, so `damage - dist * falloff` is free. Under **A1** this comes at no storage cost at all.

**Tests required.** Blast hits all entities in radius; wall-shadowed tiles spared; friendly fire on/off; caster killed by own blast leaves a sane turn order (D4 regression); AoE kill removes multiple entities from the turn order in one call; game-over triggered by a multi-kill; preview updates on hover and clears on mode change; preview does not mutate game state.

---

### F2 — Knockback and environmental kills
**`geometry_line_iter` (Bresenham) + the unused chasm tile (C2) + `skill_t` → shove enemies into the void**

**Impact 4 · Effort 2 · Confidence high**

`skill_t` gains `int knockback`. On hit, take the attacker→target direction, and walk the target that many tiles using `geometry_line_iter` (`geometry.c` — already pure, already tested via LOS), stopping at a non-walkable tile or an occupied tile. If it stops *on* a non-walkable, non-sight-blocking tile (a chasm, **C2**/**F4**), the unit dies outright. Collision with another unit damages both.

**Why it is good.** It converts terrain from an obstacle into a weapon. A weak, cheap push skill becomes situationally lethal, which is exactly the kind of decision that makes positioning matter. It composes multiplicatively with F1 (a blast that shoves everything outward from the impact tile) — and both are pure functions of already-tested primitives.

**Tests required.** Push into open ground / into a wall (stops short) / into another unit (both damaged, neither moves through) / into a chasm (dies, tile freed, turn order compacted — D4 again) / off the grid edge; knockback path uses the same Bresenham stepping as LOS.

---

### F3 — Path preview and movement cost on hover
**Distance field from A1 + hover (C3) → the tactics UI table stake this game is missing**

**Impact 4 · Effort 2 · Confidence high**

Once **A1** caches the distance field instead of a tile list, `dist[hover]` *is* the MP cost of moving there — already computed, currently discarded. Gradient descent from the hovered tile back to the mover (repeatedly step to a neighbour with `dist == current - 1`) reconstructs the actual path in a handful of lines, with no extra allocation and no new search.

Render it as a chain of dithered squares (**C5**), and show the cost as pips or a number.

**Why it is non-obvious.** It looks like a new feature requiring new pathfinding. It is a *rendering* change: A1 makes the data already sit in memory. This is the concrete payoff that justifies A1 on its own.

**Tests required.** Path length equals the reported cost; path avoids walls and occupied tiles; hovering an unreachable tile shows no path; hovering the mover's own tile is empty; path is stable (deterministic tie-break) across frames.

---

### F4 — Chasms, low walls and windows — one line of scenario code
**The unused `(walkable=false, blocks_sight=false)` combination (C2) → new terrain, no engine work**

**Impact 3 · Effort 1 · Confidence high** *(blocked on **D3**)*

`grid_set_walkable(grid, p, false)` **without** `grid_set_blocks_sight(grid, p, true)` already means "you can see across it but not walk on it". The engine supports it today; nothing uses it; and **D3** currently makes it behave wrongly (range is eaten by the geodesic detour).

Once D3 is fixed you get, for free:
- **Chasms** — ranged units dominate; melee must go around. Instant map-design vocabulary. Doubles as the kill zone for **F2**.
- **Low walls / windows** — shoot over, walk around.
- **Water / lava** — same bits, different colour and a hazard hook.

Add the named archetypes from **A6** plus a render colour in `render_tiles` (`render.c:61-71`, which already branches on both bits and just needs a fourth case).

**Tests required.** D3's chasm repro (inverted); pathing routes around a chasm; LOS crosses it; a full-width chasm with a single bridge produces the expected reachable set.

---

### F5 — ★ Threat overlay ("where can they hit me next turn?")
**`pathing_compute_distances` per enemy (C4) + range check + `render_cache` → the highest depth-per-line feature available**

**Impact 5 · Effort 2 · Confidence high**

For each live enemy: flood fill from its position capped at its `max_mp`, then for every tile in that field union in everything within its best skill's range with LOS. The result is the set of tiles where the player can be attacked next turn.

**This is the exact computation `ai_run_ennemy_turn` already performs** (`ai.c:147-165`) — move toward the target, then attack with the best in-range skill. Rendering it simply *shows the player what the AI is about to do*.

**Why it is the best value in this list.** It converts an opaque AI into a readable one. Every move becomes an informed decision instead of a guess, and the difference between a good and a bad player becomes visible. It requires **no new game rules** — it is pure visualisation of existing logic. Toggle it with a HUD button (or a hover-hold), render with `graphics_draw_rectangle_dithered` (**C5**) in a warning colour, and stack intensity where multiple enemies overlap.

**Design note.** This also becomes an honest AI-correctness test: if the overlay says a tile is safe and a unit is hit there, either the overlay or the AI is wrong — and both are pure functions, so the invariant can be asserted directly (**T1**).

**Tests required.** Threat set matches what the AI actually reaches on the following turn (assert over a full end-turn, across several scenarios); walls and LOS respected; a 0-MP enemy contributes only its static range; dead enemies contribute nothing; overlay clears on toggle-off.

---

### F6 — Overwatch / reaction fire
**Step-wise movement + `pathing_line_of_sight_clear` + spare AP → interrupt the mover**

**Impact 4 · Effort 4 · Confidence medium**

Currently `action_try_move` teleports (`action.c:21-22`: `entity->position = target`). Making movement *walk the path* (which F3 already reconstructs, and which animation needs anyway) creates the hook: after each step, any hostile in overwatch stance with AP remaining and LOS to the mover's new tile spends its AP to fire.

This is the mechanic that makes cover matter *during* movement rather than only at rest, and it gives the player something to do with leftover AP other than nothing.

**Prerequisite:** stepwise movement is a real change — `action_try_move` becomes an iterator over the path, each step re-checking triggers, with `mp` decremented per step. Worth doing regardless, since it is also what animation, ZoC, and hazard tiles all need.

**Tests required.** Overwatcher fires on entry into LOS; does not fire without AP; fires exactly once per turn; a mover killed mid-path stops moving and refunds nothing; overwatch through tall grass does not trigger; mover ending outside LOS is never hit.

---

### F7 — Status effects, ticked at the turn boundary that already exists
**`turn_advance` (already the per-entity turn boundary) + `skill_t` extension → poison, stun, slow, guard**

**Impact 4 · Effort 3 · Confidence high**

`turn_reset_points` (`turn.c:27-30`) is called from `turn_advance` on every entity as it becomes active — it is *already* the canonical "this unit's turn begins" hook, and it already mutates the entity's per-turn resources. That is exactly and only where status effects belong.

`entity_t` gains `int poison_turns, stun_turns, guard_turns` (or a small `status_t` bitfield + counters); `turn_reset_points` applies poison damage, zeroes AP/MP when stunned, and decrements every counter. `skill_t` gains an `effect` field applied on hit.

**Why it is non-obvious.** It looks like it needs a new scheduler. It does not — the scheduler exists and is three lines long. The effort is in the UI (showing status on a unit) and in teaching the AI to value it, not in the engine.

**Tests required.** Poison ticks exactly once per own-turn and kills at 0 HP (turn order compacted — D4); stun zeroes AP/MP and expires; effects tick on death correctly; effect from a dead applier still resolves; durations survive save/replay (**F8**).

---

### F8 — ★ Replay, undo, and one-click reproducible crash reports
**Determinism (C1) + `linear_allocator_copy` + the existing trap overlay → three features from one property**

**Impact 5 · Effort 3 · Confidence high**

Three capabilities fall out of one architectural fact — the game is a pure function of its event stream.

**(a) Undo.** Snapshot the arena into a small ring buffer before each state-mutating event using `linear_allocator_copy` (`memory.c:71-75`, already unit-tested). Undo = restore. This directly mitigates **D1**-class harm: a misclick stops being unrecoverable. For a game with no randomness this is exact, not approximate.

**(b) Replay.** Record the event stream (12 bytes/event). Playback re-runs `game_on_input_event` from the same scenario and reproduces the match **bit for bit** — no state snapshots, no desync risk. A 200-action match is 2.4 KB. Enables shareable matches, AI regression corpora, and tutorial scripting.

**(c) ★ The killer application: self-reproducing bug reports.** `main.js` already renders a trap overlay **with a Copy button** (`main.js:27-38, 58-63`) and already symbolicates wasm traps to `file:line` via the dev server. Attach the event log to that overlay, and every crash report becomes a *runnable repro* — paste the log into a test and the failure recurs deterministically. Combined with **D5** (real panic messages), a bug report becomes: assertion text, source location, and the exact input sequence that produced it.

**Implementation.**
1. Ring buffer of `input_event_t` in `app_state_t`, appended in `app_dispatch_input_events` (`app.c:56-69`).
2. Base64 it into the overlay text; a `replay(events)` export feeds it back through `game_on_input_event`.
3. Undo: a fixed-count arena snapshot ring, sized at init. **Much simpler after A1**, since without variable-length overlays the live arena region is fixed-size.

**Tests required.** Replaying a recorded log reproduces byte-identical final state (hash the arena); undo restores exact prior state; undo/redo across a turn boundary; ring buffer wraps without corruption; a replay that ends in a trap re-traps at the same event index.

---

### F9 — Dynamic initiative timeline
**`turn_state_t.capacity` (C7) + the existing `render_timeline` → a living turn queue**

**Impact 4 · Effort 3 · Confidence medium**

Turn order is a fixed authored array with a cursor (`turn.h:10-14`, `scenario.c:63-70`), and the HUD already draws it (`render.c:178-196`). Two pieces of the puzzle are in place; the missing one is that order never changes.

Give `entity_t` a `speed`, accumulate initiative, and rebuild the order each round. The **same** HUD widget then becomes a live "who acts next" queue — the central information display of every modern tactics game — with no new rendering work. `turn_state_t` already separates `order` from `capacity` (**C7**), which is exactly the storage a mutable queue needs.

Second-order payoff: haste/slow become the most interesting status effects in **F7**, because their consequence is *visible* in the timeline before it happens.

**Tests required.** Order matches speed ranking; ties break deterministically; a dead entity leaves the queue (D4); speed changed mid-round reorders correctly from the next round; the HUD reflects the live order.

---

### F10 — AI archetypes from the existing scoring predicate
**`ai_skill_beats` (C6) + the threat map (F5) inverted → enemies that are not all identical**

**Impact 4 · Effort 3 · Confidence medium**

Every enemy today runs the same script: find the nearest player, charge, attack (`ai.c:147-165`). It is exploitable (kite them into a corridor and win every time) and makes all encounters feel the same.

`ai_skill_beats` (`ai.c:105-110`) is already a clean comparator, already parameterising two searches. Generalise it from comparing *skills* to scoring *candidate actions* — `(destination tile, skill, target)` — and the AI becomes:

```c
score = w_damage * expected_damage
      + w_kill   * (target_dies ? 1 : 0)
      + w_safety * -incoming_threat_at(destination)   // F5's map, applied to itself
      + w_range  * prefers_distance;
```

Different weight vectors give: **Brute** (charge, maximise damage), **Skirmisher** (maximise damage while minimising incoming threat — naturally kites), **Guardian** (stay adjacent to a weak ally), **Artillery** (maximise AoE targets hit — needs F1). One `ai_profile_t` field per entity, chosen in the scenario.

The candidate set is bounded by the already-computed reachable field (**A1**) × skills × targets, so it is small and needs no new search.

**Tests required.** Each archetype produces its characteristic move in a scripted scenario; scoring is deterministic; a skirmisher retreats when threatened; the AI never selects an illegal action (fuzz with **T2**).

---

### F11 — Bitmap text, and the tooltips it unlocks
**A static font table + `graphics_draw_rectangle` + hover (C3) → the game can finally say numbers**

**Impact 4 · Effort 2 · Confidence high**

There is no text rendering anywhere. HP is a bar, AP/MP are pips, skills are unlabelled coloured rectangles (`render.c:231-235`), and damage is invisible. Players are reading colours and counting squares.

A 5×7 bitmap font is a `static const uint8_t[96][7]` table plus a ~20-line glyph blitter over the existing `graphics_draw_rectangle`. No file loading, no dependency, fully freestanding, ~2 KB of data.

Unlocks immediately: HP as `7/10`, floating damage numbers, skill labels with damage/range/AP cost, turn counter, and — combined with **C3** — a hover tooltip showing the unit under the cursor. It also makes **F1**'s blast preview readable ("3 targets, 5 dmg each") rather than merely coloured.

**Tests required.** Glyph rendering is pixel-exact for a known string (golden hash, **T3**); text clips at the framebuffer edge (**H4**); unknown characters render a placeholder rather than reading out of the table.

---

### F12 — Zone of control
**One condition inside `pathing_bfs`'s neighbour loop → movement has consequences**

**Impact 3 · Effort 2 · Confidence high**

`pathing_bfs` already blocks on entities (`pathing.c:70-72`). Adding "a tile adjacent to a live hostile terminates the flood fill" (do not expand *from* it) means units cannot freely run past enemies. This is the cheapest possible source of tactical depth — a handful of lines — and it makes melee units matter by giving them territorial control rather than just damage.

Composes with **F6** (overwatch punishes leaving a ZoC) and **F5** (the threat overlay must account for it).

**Tests required.** Movement stops on entering a ZoC; a corridor guarded by one unit is impassable; allies do not exert ZoC; a dead unit's ZoC disappears; the reachable overlay matches what movement actually permits (an invariant for **T1**).

---

## 9. Top critical issues

Ranked by `impact × reachability`.

| # | Issue | Status | Impact | Effort | Why it ranks here |
|---|---|---|---|---|---|
| 1 | **D1** — movement fires in attack mode | CONFIRMED (repro) | 5 | 1 | Player-visible today. Unrecoverable misclick. Its fix is the AoE hook. |
| 2 | **D2** — `SLICE_AT` off-by-one, no bounds checks in Release | CONFIRMED (repro) | 5 | 1 | The one guard protecting every array in the codebase, and it is wrong. Corrupts silently in Release. |
| 3 | **D3** — range is geodesic, documented as Manhattan | CONFIRMED (repro) | 4 | 2 | Silent no-op attack with no feedback. Blocks F4. Fixing it is also a ~160× speedup (P1). |
| 4 | **D4** — turn order desyncs when the active entity dies | CONFIRMED (repro) | 4 | 1 | Dormant now; **AoE (F1) activates it**. Fix before shipping F1. |
| 5 | **H4** — no clipping in `graphics.c` | CONFIRMED (code) | 4 | 2 | Uncapped HUD loops + no Release bounds checks = memory corruption on any content change. |
| 6 | **H3** — AI can loop forever on a failed move | HYPOTHESIS | 5 | 1 | Low likelihood, catastrophic outcome (tab hang, no diagnostic). One-line guard. |
| 7 | **H5** — unbounded event write bypasses the arena bound | CONFIRMED (code) | 3 | 2 | The only place external data enters, and the only place the allocator's guard is bypassed. |
| 8 | **H2** — null skill deref reads address 0 in wasm | HYPOTHESIS | 3 | 1 | Does not crash — silently misbehaves, which is worse to diagnose. |

---

## 10. Best quick wins

Highest value per unit of effort. Each is ≤ ~1 hour.

| # | Change | Impact | Effort | Payoff |
|---|---|---|---|---|
| 1 | **D1** dispatch split | 5 | 1 | Fixes the reported bug *and* creates the tile-target hook AoE needs. |
| 2 | **D2** `result < s.end` | 5 | 1 | Restores the codebase's only bounds check. Expect it to surface further latent bugs — that is the value. |
| 3 | **P1/D3** direct range check | 4 | 1 | Less code, ~160× faster, fixes a gameplay bug. Strictly better on all three axes. |
| 4 | **D4** cursor fallback | 4 | 1 | Removes the landmine that AoE would step on. |
| 5 | **H3** check `action_try_move`'s return | 5 | 1 | Turns a potential infinite hang into a no-op. |
| 6 | **H2** null guard in `ai_run_ennemy_turn` | 3 | 1 | One line. |
| 7 | **T4** run the suite in Release | 4 | 1 | Validates the program you actually ship. Pairs with D2/H4. |
| 8 | **F4** chasm terrain | 3 | 1 | New tactical terrain for one scenario line, once D3 lands. |
| 9 | **D5** wire `report_panic` | 3 | 2 | Real `file:line: assertion` messages; deletes a symbolication round-trip. Infra already written. |
| 10 | **P4** `requestAnimationFrame` | 2 | 1 | Correct pacing, free background throttling, mitigates H5. |
| 11 | **H6** bind to `127.0.0.1` | 2 | 1 | One argument. |
| 12 | **T1** invariant assertions | 5 | 2 | Catches D1's whole bug class permanently. |

---

## 11. Best original feature proposals

Ranked by depth-per-line and by how hard they are to spot from a surface reading.

**1. F5 — Threat overlay.** *(Impact 5, Effort 2.)* The best value in the report. It adds **zero new rules** — it renders a computation `ai_run_ennemy_turn` already performs — and it transforms the game from guesswork into decision-making. It doubles as an executable AI-correctness invariant.

**2. F8 — Replay / undo / self-reproducing crash reports.** *(Impact 5, Effort 3.)* The deepest hidden capability. The core is already a pure function of a 12-byte event stream, `linear_allocator_copy` already exists and is tested, and `main.js` already has a trap overlay with a Copy button. Three features are sitting one wire away from each other. The crash-report application in particular is something most projects cannot have at any price.

**3. F1 — AoE with cover-aware blast preview.** *(Impact 5, Effort 3.)* Your idea, with the non-obvious part: the blast footprint is `pathing_compute_line_of_sight` rooted at the impact tile, so explosions respect walls and corners **for free**. The cover system that shapes shooting now shapes explosions too.

**4. F3 — Path preview and MP cost on hover.** *(Impact 4, Effort 2.)* Looks like new pathfinding; is actually a rendering change once **A1** keeps the distance field. This is A1's concrete dividend.

**5. F2 — Knockback into chasms.** *(Impact 4, Effort 2.)* Two unused things — the Bresenham walker and the fourth tile-bit combination — combine into environmental kills. Turns terrain from obstacle into weapon.

**6. F9 — Dynamic initiative timeline.** *(Impact 4, Effort 3.)* The HUD widget exists; `turn_state_t` already separates `order` from `capacity`. The scaffolding for a live turn queue is 80% built and unused.

**7. F7 — Status effects at the existing turn boundary.** *(Impact 4, Effort 3.)* `turn_reset_points` is already the per-unit turn hook and already mutates per-turn resources. The "scheduler" this appears to need is three lines that already exist.

**8. F11 — Bitmap text.** *(Impact 4, Effort 2.)* ~2 KB of static data unlocks damage numbers, skill labels and hover tooltips, and makes every other feature here legible.

---

## 12. Prioritised implementation roadmap

Ordered so each phase unblocks the next and nothing is built on a known-broken foundation.

### Phase 0 — Stop the bleeding (~half a day)
> Rationale: these are the defects that corrupt state or mislead the player, and several are prerequisites for everything after.

1. **D2** — `slice_at` bound to `<`. *Do this first*; fixing it may reveal further latent bugs that change later work.
2. **T4** — add a Release pass to `npm test`. Needed to validate D2 and H4 where it matters.
3. **D1** — split `game_on_tile_pressed` into movement/target handlers.
4. **D4** — active-entity cursor fallback.
5. **H3** — check `action_try_move`'s return in the AI loop.
6. **H2** — null-guard `ai_preferred_skill`.
7. **H1** — assert non-empty turn order; hoist the game-over check in `game_advance_turn`.

*Exit criteria:* 106 tests green in Debug **and** Release; four new regression tests (one per confirmed defect) that fail before the fix.

### Phase 1 — Make correctness self-enforcing (~1 day)
> Rationale: Phase 0 fixed four bugs that 100% coverage missed. Without this phase the next four will be missed too.

8. **T1** — `assert_game_invariants`, called after every event in tests.
9. **T2** — deterministic fuzz driver, several seeds.
10. **D5** — wire `report_panic` so failures carry `file:line: assertion`. Makes T2's failures immediately actionable.
11. **H4** — clipping in `graphics.c`; clamp the HUD pip and timeline loops.
12. **H5** — bound the input-event write on both sides; coalesce mouse moves.
13. **H6** — bind to localhost; validate the symbolicate payload.

*Exit criteria:* fuzz driver survives 20k events × 5 seeds with all invariants holding, in Debug and Release.

### Phase 2 — Fix the foundations (~2–3 days)
> Rationale: A1 must land before the overlay count grows, or every new overlay pays the pointer-rebasing tax. D3 must land before terrain expands.

14. **D3 / P1** — settle the range metric; direct `skill_target_in_range`; promote `pathing_line_of_sight_clear`; correct the header docs.
15. **A1** — cache the distance field instead of tile lists. Delete `game_scratch_push`, the `shift` plumbing, and `render_cache_assert_layout`.
16. **P2** — occupancy bitmap in `pathing_bfs`.
17. **A2** — single `layout_hit_test` shared by input and render.
18. **A6 / F4** — name the tile archetypes; add chasm rendering and a chasm to the default scenario.

*Exit criteria:* `game_set_mode` and every `game_on_*` handler return `void`; the arena never grows after `game_init`; suite green.

### Phase 3 — The features you asked for (~3–4 days)
19. **F1** — AoE skills, `game_on_tile_targeted`, friendly fire, blast preview on hover.
20. **F3** — path preview and MP cost (near-free after A1).
21. **F5** — threat overlay.
22. **A3** — skill bar sized to the entity; keyboard skill selection (needs a `KEY_DOWN` event type in `runtime.h`).
23. **F11** — bitmap font; damage numbers and skill labels.

*Exit criteria:* an AoE skill that can kill its own caster without corrupting the turn order; threat overlay validated against actual AI behaviour by an invariant test.

### Phase 4 — Depth (~1 week)
24. **A4** — dynamic entity list (reserve capacity), unblocking summons and reinforcements.
25. **A5** — data-driven scenarios.
26. **F8** — replay, undo, crash-report event log.
27. **F2** — knockback and environmental kills.
28. **F7** — status effects.
29. **F9** — dynamic initiative timeline.
30. **F10** — AI archetypes.
31. **F12** — zone of control.
32. **F6** — step-wise movement and overwatch.
33. **P3** — dirty-flag rendering (pairs with movement animation).

---

## 13. Implementation-ready backlog

Each item is scoped to be picked up cold. `Impact / Effort / Confidence`.

| ID | Title | Files | Definition of done | I/E/C |
|---|---|---|---|---|
| **B01** | Tighten `slice_at` bound to `<` | `src/lib/memory.c:80` | `SLICE_AT(s, count)` panics; `slice_at_panics_on_index_equal_to_count` added; guard-word test proves no neighbour write; suite green in Debug **and** Release | 5/1/certain |
| **B02** | Split tile dispatch by mode | `src/game/game.c:272-293` | `game_on_tile_pressed` handles MOVEMENT only; `game_on_tile_targeted` stub for ATTACK; D1 repro inverted; existing movement tests pass | 5/1/certain |
| **B03** | Fix active-entity turn cursor | `src/game/turn.c:50-71` | Active entity dies → next entity in original order acts; wraparound case covered; `game_turn_order_compacts_...` still passes | 4/1/certain |
| **B04** | Guard AI move failure | `src/game/ai.c:64-101,155` | `ai_step_toward` returns `bool`; loop breaks on failure; boxed-in-enemy test returns within one turn | 5/1/high |
| **B05** | Null-guard preferred skill | `src/game/ai.c:153` | Skill-less enemy no-ops instead of dereferencing 0; test with a skill-less entity | 3/1/high |
| **B06** | Assert non-empty turn order; hoist game-over check | `src/game/turn.c:38`, `src/game/game.c:224-236` | `turn_active_entity` asserts; `turn_advance` not called after game over | 4/1/medium |
| **B07** | Release test pass | `package.json`, `server/build.js` | `npm test` builds and runs Debug **and** Release; both green | 4/1/certain |
| **B08** | Direct `skill_target_in_range` | `src/game/skill.c`, `src/game/pathing.{c,h}` | Manhattan + LOS; `pathing_line_of_sight_clear` promoted to `PUBLIC`; header docs corrected; chasm repro inverted; all ranged tests pass | 4/2/high |
| **B09** | Diamond broad phase for the range overlay | `src/game/pathing.c:113-135` | Overlay matches B08's predicate exactly (invariant test over the whole grid) | 3/2/high |
| **B10** | Clip in `graphics.c` | `src/lib/graphics.c` | Rects straddling all four edges paint only in-bounds pixels; off-screen and negative-size rects write nothing; guard allocations unchanged | 4/2/high |
| **B11** | Clamp HUD pip and timeline loops | `src/game/render.c:184-195,250-258` | Loops bounded by available width; overflow indicator; test with high AP and many entities | 3/1/high |
| **B12** | Bound the input-event write | `src/game/input.c`, `web/wasm-shared.js:86-101` | C passes capacity; JS clamps and keeps the remainder queued; over-capacity test asserts the arena bound holds and the remainder arrives next poll | 3/2/high |
| **B13** | Coalesce mouse moves | `web/wasm-shared.js:52-59` | Consecutive moves collapse to the last; click ordering preserved relative to moves | 2/1/high |
| **B14** | Wire `report_panic` | `src/lib/assert.{c,h}` | `panic_at(cond, __FILE__, __LINE__, #cond)`; overlay shows file:line:assertion; `expect_panic`/`expect_trap` unaffected; JS spy test | 3/2/high |
| **B15** | Fix `test_run` arena ceiling | `src/test.c:82-93` | Arena end ≤ end of linear memory; over-allocation panics cleanly instead of trapping | 2/1/high |
| **B16** | Lock down the dev server | `server/server.js:88-133`, `server/symbolicate.js` | Binds `127.0.0.1`; Host check on `/__symbolicate`; `frames` shape and length validated before spawning | 2/1/high |
| **B17** | Game invariant assertions | new `src/test_invariants.h` | `assert_game_invariants` covering turn/entity agreement, tile uniqueness, bounds, point ranges, mode↔overlay consistency, game-over consistency; called from every game test | 5/2/high |
| **B18** | Deterministic fuzz driver | new `src/test_fuzz.c` | 20k random events × 5 seeds, invariants hold, in Debug and Release; seed printed on failure | 5/2/high |
| **B19** | Golden-image render tests | `src/test_render.c` | FNV-1a framebuffer hash; one pinned hash per scenario state | 3/2/high |
| **B20** | ★ Cache the distance field | `src/game/render_cache.{c,h}`, `game.c`, `app.c` | `render_cache_t` holds fixed `W*H` fields allocated at `game_init`; `game_scratch_push`, the `shift` returns, `app_dispatch_input_events` rebasing and `render_cache_assert_layout` all deleted; arena never grows post-init; suite green | 5/3/high |
| **B21** | Occupancy bitmap in pathing | `src/game/pathing.c` | `entity_find_at` calls removed from BFS and raycast inner loops; behaviour identical | 3/2/high |
| **B22** | Unified hit testing | `src/game/layout.c`, `game.c:341-397`, `render.c:224-236` | One `layout_hit_test`; the duplicated clamp and its sync comments deleted | 3/3/high |
| **B23** | Tile archetypes + chasm rendering | `src/game/grid.{c,h}`, `render.c:61-71`, `scenario.c` | Four named archetypes; chasm colour; a chasm in the default scenario; pathing routes around, LOS crosses | 3/1/high |
| **B24** | `requestAnimationFrame` loop | `web/wasm-shared.js:156-162` | rAF-driven; `clock_time_to_wait` retained as the fixed-timestep gate | 2/1/certain |
| **B25** | AoE skills | `entity.h`, `action.{c,h}`, `game.c`, `skill.c` | `skill_t.aoe_radius`/`friendly_fire`; `action_try_attack_area`; blast = LOS field at the impact tile; caster can die without corrupting the turn order (needs B03) | 5/3/high |
| **B26** | AoE hover preview | `game.c`, `render_cache`, `render.c` | Third overlay field (needs B20); dithered render; updates on hover, clears on mode change; no state mutation | 4/2/high |
| **B27** | Path preview + MP cost | `render.c`, `render_cache` | Gradient descent over the cached field (needs B20); path length equals reported cost; deterministic tie-break | 4/2/high |
| **B28** | Threat overlay | new `src/game/threat.c`, `render.c`, HUD toggle | Per-enemy reach ∪ skill range with LOS; invariant test asserting the AI never attacks outside the overlay | 5/2/high |
| **B29** | Skill bar + keyboard selection | `layout.h:11`, `runtime.h:10-13`, `game.c`, `render.c`, `web/main.js` | `VIEWPORT_MAX_SKILL_BUTTONS` cap removed; `INPUT_EVENT_KEY_DOWN` added; keys 1..9 select skills; `game_skill_button_hit_test_clamps_more_than_two_skills` replaced | 3/2/high |
| **B30** | Bitmap font | new `src/lib/font.{c,h}`, `render.c` | 5×7 glyph table; clipped blitter; HP numbers, damage numbers, skill labels; golden-hash test | 4/2/high |
| **B31** | Dynamic entity list | `src/game/entity.{c,h}` | Capacity reserved at init; the append-in-place assert and its "For now" comment removed; spawn-after-setup test | 3/3/high |
| **B32** | Data-driven scenarios | `src/game/scenario.{c,h}` | `scenario_desc_t` POD + one loader; default scenario expressed as a static table; the six-fold skill copy-paste deleted | 3/3/high |
| **B33** | Event-log recording + crash repro | `app.{c,h}`, `web/main.js` | Ring buffer of events; base64 in the trap overlay; `replay()` export; replay reproduces byte-identical final state | 5/3/high |
| **B34** | Arena snapshot undo | `app.{c,h}`, `game.c` | Fixed-count snapshot ring via `linear_allocator_copy` (simpler after B20); undo restores exact prior state across turn boundaries | 4/3/high |
| **B35** | Knockback | `entity.h`, `action.c`, `geometry.c` | `skill_t.knockback`; Bresenham push; stops at wall/unit; chasm = death + tile freed + turn order compacted | 4/2/high |
| **B36** | Status effects | `entity.h`, `turn.c:27-30`, `action.c` | Poison/stun/slow ticked in `turn_reset_points`; expiry; death-by-poison compacts the turn order | 4/3/high |
| **B37** | Dynamic initiative | `turn.{c,h}`, `entity.h`, `render.c:178-196` | `entity_t.speed`; order rebuilt per round; deterministic ties; existing HUD shows the live queue | 4/3/medium |
| **B38** | AI archetypes | `src/game/ai.c` | `ai_profile_t` weight vectors; scored candidate actions; Brute/Skirmisher/Guardian behave characteristically in scripted tests; fuzz-clean | 4/3/medium |
| **B39** | Zone of control | `src/game/pathing.c:52-77` | Movement halts on entering a hostile ZoC; reachable overlay matches actual movement (invariant); allies exert none | 3/2/high |
| **B40** | Step-wise movement + overwatch | `action.c:7-25`, `game.c`, `entity.h` | Movement walks its path, MP per step; overwatch fires once per turn on LOS entry; mover killed mid-path stops | 4/4/medium |
| **B41** | Dirty-flag rendering | `app.c:71-92`, `game.h` | Render and present skipped when nothing changed; a test asserts no present on a no-op click | 2/3/high |

---

## 14. Verification notes

- Baseline `npm test`: **106/106 pass**, zero coverage gaps.
- Probes for **D1**, **D2**, **D3**, **D4** were compiled into `src/test_game_movement.c`, built with `node server/build.js --tests`, executed via `node server/run-tests.js`, and each **passed**, confirming the described (incorrect) behaviour. All four probes were reverted with `git checkout src/test_game_movement.c`.
- **Working tree is clean** apart from this file; `git status --short` shows only `REPORT.md`.
- The probe sources are reproduced inline in sections **D1–D4** and can be pasted back into `src/test_game_movement.c` (append to `g_game_movement_tests[]`) to re-verify at any time.
