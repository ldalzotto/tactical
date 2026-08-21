# F1-01: Add `aoe_radius` to `skill_t` and a demo AoE skill

## Context
Part of F1 (area-of-effect skills with a cover-aware blast preview). `skill_t` is
currently a plain POD with no AoE concept:

```c
// src/game/entity.h:16-20
typedef struct {
    int range;
    int damage;
    int ap_cost;
} skill_t;
```

`SKILL_MELEE` and `SKILL_RANGED` are declared `extern` in `src/game/skill.h:11-12`
(and re-declared in `src/game/action.h:11-12` — pre-existing duplicate extern,
not in scope here) and defined in `src/game/skill.c:5-6`:

```c
const skill_t SKILL_MELEE = { .range = 1, .damage = 5, .ap_cost = 1 };
const skill_t SKILL_RANGED = { .range = 3, .damage = 3, .ap_cost = 1 };
```

Entities own skills via a shared, contiguous `slice_skill_t` list
(`entity.h:29,41-43`), wired up per-entity in `src/game/scenario.c` via
`skill_list_add(allocator, &skills, SKILL_X)` calls (see `scenario.c:33-59`
for the existing pattern across several entity definitions).

This ticket adds the field and a concrete AoE skill so the feature is
reachable through the real game once later tickets land, not just unit
tests. It does not change any behavior by itself (no code yet reads
`aoe_radius`).

## Scope / implementation guidance
- Add `int aoe_radius;` to `skill_t` in `src/game/entity.h`. `0` means
  single-target (the existing behavior); this is the implicit default for
  `SKILL_MELEE`/`SKILL_RANGED`'s designated initializers, so no changes
  needed to those two constants.
- Add a new skill constant in `src/game/skill.h`/`skill.c`:
  ```c
  const skill_t SKILL_FIREBALL = { .range = 4, .aoe_radius = 2, .damage = 4, .ap_cost = 1 };
  ```
- Wire `SKILL_FIREBALL` into at least one player-controlled entity's skill
  list in `src/game/scenario.c`, following the existing
  `skill_list_add(allocator, &skills, SKILL_X)` pattern used for that
  entity (e.g. append it alongside `SKILL_MELEE`/`SKILL_RANGED` for one of
  the player entries around `scenario.c:33-39`).

## Acceptance criteria
- `skill_t` has an `aoe_radius` field; `SKILL_MELEE`/`SKILL_RANGED` still
  have `aoe_radius == 0`.
- `SKILL_FIREBALL` exists, is declared `extern` in `skill.h`, defined in
  `skill.c`, and has `aoe_radius > 0`.
- At least one player entity in `scenario.c` has `SKILL_FIREBALL` in its
  skill list.
- Project still builds; no existing test behavior changes (nothing yet
  reads `aoe_radius`).

## Testing
- Build only. No new tests required for this ticket — `aoe_radius` isn't
  consumed by any code path yet, so there's nothing behavioral to assert.

## Dependencies
None — this is the foundation ticket for F1.

## Non-goals
- No damage-falloff-by-distance field or logic (deferred; the blast
  footprint computed in F1-02 doesn't carry per-tile distance yet).
- No `friendly_fire` field — out of scope for this iteration of F1 (AoE
  damage will always exclude the caster's own team, matching
  `action_try_attack`'s existing same-team rejection).
