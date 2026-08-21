# F1-00: Assumptions and confirmed decisions

## Context
F1 (area-of-effect skills with a cover-aware blast preview, no friendly
fire) was scoped across tickets F1-01 through F1-08. The decisions below
were confirmed during planning but are referenced rather than
re-litigated in each individual ticket. Any implementer picking up an
F1-0x ticket should read this file first — it's the shared contract the
tickets assume.

## Confirmed decisions

- **No friendly fire in this iteration.** AoE damage always excludes the
  attacker's own team (including the attacker itself), matching
  `action_try_attack`'s existing same-team rejection. This is why
  `turn_remove_dead_entity`'s `assert_debug(dead != active)`
  (`src/game/turn.c:58-59`) is never at risk from this feature: the
  currently-active entity is always on the attacker's team, so it can
  never appear among an AoE blast's casualties. A `friendly_fire` field on
  `skill_t` and the turn-cursor fix that would be needed to support it are
  explicitly out of scope — see F1-03's non-goals.

- **Targeting: click any in-range tile.** AoE skills are cast by clicking
  any tile within the attacker's `skill.range`/LOS — empty or occupied —
  which becomes the blast center. This differs from single-target skills,
  which keep today's entity-only-click behavior unchanged. See F1-04.

- **A demo skill is added, not just infrastructure.** `SKILL_FIREBALL`
  (`range = 4, aoe_radius = 2, damage = 4, ap_cost = 1`) is added and wired
  into at least one player entity in `src/game/scenario.c`, so the feature
  is reachable end-to-end through the real game, not only through unit
  tests. See F1-01.

- **Preview overlay coexists with attack range, it doesn't replace it.**
  While hovering a valid AoE impact tile in attack mode, the blast preview
  (`blast_preview_tiles`) renders on top of the existing
  `attack_range_tiles` overlay rather than swapping it out. This is why
  `render_cache_t` needs a third, independently-toggled region instead of
  extending the existing two-region mutual-exclusivity model. See F1-05
  and F1-06.

## Explicit non-goals across all of F1
- Damage falloff by distance from impact (the blast footprint helper,
  `pathing_compute_blast_tiles`, intentionally returns a flat tile list
  with no per-tile distance value — see F1-02's non-goals).
- Friendly fire and the associated turn-cursor/active-entity-death
  handling (see above).
- Pixel-level rendering assertions for the new overlay (see F1-08's
  non-goals) — tests assert against `render_cache`/game state, matching
  how the existing attack-range overlay is tested.

## Dependencies
None — this is a reference document, not an implementation ticket.
