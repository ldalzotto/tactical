# F1-03: `action_try_attack_area` — apply AoE damage

## Context
Part of F1. The existing single-target attack action,
`src/game/action.h:15-21` / impl `src/game/action.c:27-47`:

```c
PUBLIC bool action_try_attack(grid_t grid, slice_entity_t entities, entity_t* attacker, skill_t skill, entity_t* defender) {
    assert_debug(attacker->alive);
    assert_debug(defender->alive);
    if (attacker->team == defender->team) return false;
    if (attacker->ap < skill.ap_cost) return false;
    if (!skill_target_in_range(grid, entities, attacker, skill, defender)) return false;
    attacker->ap -= skill.ap_cost;
    entity_damage(defender, skill.damage);
    return true;
}
```

`skill_target_in_range` (`src/game/skill.h:14-18`) wraps `pathing_in_range`
against a specific defender's position. `entity_damage`
(`src/game/entity.c:77-83`) clamps hp to 0 and sets `alive = false`.

This ticket adds the AoE counterpart, targeting a tile (the impact/blast
center) instead of a specific defender, and hitting every valid entity in
the blast footprint from F1-02. No friendly fire: only entities with
`team != attacker->team` are damaged, exactly like `action_try_attack`'s
same-team rejection — this means the currently-active entity (always the
attacker's own team) can never die from its own blast, so
`turn_remove_dead_entity`'s existing invariant
(`assert_debug(dead != active)`, `turn.c:58-59`) is never violated by this
feature. That assert's comment literally names AoE as the future case that
would break it; excluding friendly fire keeps this iteration out of that
problem.

## Scope / implementation guidance
- Add to `src/game/action.h`:
  ```c
  PUBLIC bool action_try_attack_area(linear_allocator_t *allocator, grid_t grid, slice_entity_t entities, entity_t *attacker, skill_t skill, position_t impact, slice_entity_ptr_t *out_hit);
  ```
- Implement in `src/game/action.c`:
  - `assert_debug(attacker->alive)`.
  - `assert_debug(skill.aoe_radius > 0)` — this function is only valid for
    AoE skills; single-target skills keep using `action_try_attack`.
  - Return `false` (no state change) if `attacker->ap < skill.ap_cost`.
  - Return `false` if `!pathing_in_range(grid, entities, attacker->position, impact, skill.range)`
    — range+LOS check from attacker to the impact tile, reusing the same
    primitive `skill_target_in_range` wraps.
  - On success: `attacker->ap -= skill.ap_cost;`, compute
    `pathing_compute_blast_tiles(allocator, grid, entities, impact, skill.aoe_radius)`
    (F1-02), then for every entity in `entities` that is `alive` and on a
    blast tile and has `team != attacker->team`: call
    `entity_damage(entity, skill.damage)` and append the entity pointer to
    `*out_hit` (grow a `slice_entity_ptr_t` staged on `allocator`, same
    push-and-extend `.end` pattern used for `temp_tiles` throughout
    `game.c`, e.g. `game.c:158-170`).
  - Pop the blast-tiles temporary from `allocator` before returning (its
    caller doesn't need it — only the hit list does). `*out_hit` is left
    for the caller (`game.c`, wired in F1-04) to consume and pop.
  - Return `true`.
- Matching tiles to entities: iterate `entities`, and for each alive
  opposing-team entity check membership in the blast tile list (e.g. a
  small linear scan over the blast tiles per entity, or iterate blast tiles
  and call `entity_find_at(entities, tile)` per tile like
  `render.c:104` does for a single tile — either approach is fine given
  the small grid/entity counts in this codebase; no need to build an
  occupancy bitmap like `pathing_bfs` does for movement).

## Acceptance criteria
- `action_try_attack_area` damages every live, opposing-team entity on a
  tile within the blast footprint (F1-02) of `impact`, and no others.
- Entities on the attacker's own team (including the attacker itself, if
  it were somehow in range of its own blast) are never damaged.
- Returns `false` and makes no state change (no AP spent, no damage) when
  AP is insufficient or `impact` is out of the attacker's skill range/LOS.
- `*out_hit` contains exactly the entities that were damaged by this call
  (used by F1-04 to detect which ones died).
- AP is spent exactly once per successful call, regardless of how many
  entities the blast hits.

## Testing
Covered by F1-07 (functional AoE tests), which exercises this function
through the public game API via F1-04's dispatch wiring. This ticket does
not need a standalone test file — see this codebase's existing convention
of testing action/pathing behavior only through `game_on_input_event`
(`test_game_combat.c`, `test_game_movement.c`).

## Dependencies
- F1-01 (`skill_t.aoe_radius` field).
- F1-02 (`pathing_compute_blast_tiles`).

## Non-goals
- No friendly fire — same-team damage (including self-damage) is
  explicitly excluded in this iteration.
- No turn-order or game-over handling — this function only applies damage
  and reports who was hit; F1-04 is responsible for calling
  `turn_remove_dead_entity` per casualty and `game_check_game_over` in
  `game.c`.
