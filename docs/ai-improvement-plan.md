# Enemy AI improvement plan

Context: the enemy AI (`src/game/ai.c`) is currently a fixed, stateless,
scripted sequence per turn: find nearest reachable player, pick the
highest-damage owned skill, walk toward it, attack with whatever's in range.
No target prioritization, no perception/fog-of-war, no retreat, no squad
coordination, no difficulty levels.

## Full option space considered

Grouped by category, for reference when picking up further passes:

**A. Decision-making architecture**
1. Utility-based scoring (chosen for this pass, see below).
2. Behavior tree.
3. GOAP (goal-oriented action planning).
4. Scripted enemy archetypes (melee rusher, ranged kiter, support).

**B. Tactical behaviors**
5. Target prioritization (HP, threat, kill potential, distance).
6. Positioning (cover, avoid AoE clustering, flanking).
7. Retreat / self-preservation.
8. Resource/cooldown management.
9. Squad coordination (focus fire, avoid ally AoE overlap).

**C. Perception**
10. Line-of-sight / fog-of-war.
11. Memory of last-known player position.

**D. Difficulty & tuning**
12. Difficulty levels.
13. Per-enemy-type AI profiles / archetypes.

**E. Engineering/infra**
14. Designer-facing tunable weights/config.
15. Expanded test harness coverage.
16. Performance (recompute cost as enemy count grows).

**F. Bigger/experimental**
17. ML/RL-trained AI.
18. Per-instance AI "personality" variance.

## Decisions for this pass

- Architecture: **utility AI (scoring-based)**, not behavior tree or GOAP.
  Best fit for a single 252-line C file with no engine, a deterministic
  scenario-test harness, and a desire to fold target prioritization /
  positioning in as scoring factors rather than branchy control flow.
- Scope: architecture rewrite **and** smarter behaviors together (target
  prioritization, retreat), not a behavior-preserving port followed by a
  separate pass.
- Turns become **multi-action**: an enemy keeps acting (move + attack,
  repeated) while AP/MP remain and it can still make progress, instead of
  exactly one move-then-attack per turn.
- Out of scope for this pass: squad coordination (9), perception/fog-of-war
  (10-11), difficulty levels (12-13), designer-facing config (14), ML (17),
  personality variance (18). These remain candidate follow-ups.
- The existing scenario test suite (`src/test_game_ai.c`) is expected to
  change: multi-action turns and prioritized targeting genuinely change
  outcomes for most existing scenarios, not just internal structure.

## Task breakdown

1. **Target scoring** — replace `ai_find_nearest_player` with a scoring
   function over all reachable players: weighted by (a) lethal-hit bonus,
   (b) missing HP% (prioritize weaker targets), (c) threat (target's own
   best skill damage), (d) distance cost. Single BFS per selection, same
   cost shape as today's nearest-player scan.
2. **Retreat / self-preservation** — when the enemy's own HP% drops below a
   threshold and it isn't lined up for a kill this action, move away from
   the nearest player instead of closing/attacking, ending the turn
   defensively.
3. **Multi-action turn loop** — wrap the existing move-then-attack sequence
   in a bounded loop that re-scores and re-acts while AP/MP remain and each
   iteration makes progress (AP or MP actually spent); merge kills from
   every action in the turn into one accumulated dead-entities slice
   (`ai_run_ennemy_turn` returns exactly one batch, consumed once by
   `game_advance_turn`).
4. **AoE kill accumulation refactor** — generalize `ai_try_attack_area` to
   grow a caller-owned running `dead` accumulator (via
   `linear_allocator_insert`, same mechanism it already uses) instead of
   returning its own fresh slice, so kills across multiple actions in one
   turn — AoE and single-target mixed — land in the same accumulator.
5. **Test suite rewrite** — update `src/test_game_ai.c` scenario expectations
   for multi-action outcomes, and add new scenarios covering: target
   prioritization (low-HP / high-threat target chosen over merely-nearest),
   retreat behavior, and a multi-attack-per-turn scenario. Verify via
   `node server/build.js --tests --coverage && node server/coverage.js`.

## Explicitly deferred (candidate next passes)

- Squad coordination: focus fire, avoiding overlapping AoE on allies,
  coordinated flanking.
- Perception: line-of-sight-gated awareness, fog-of-war, last-known-position
  memory, ambush/patrol behavior when no player is seen.
- Difficulty levels and per-enemy-type AI profiles (bosses vs. grunts).
- Designer-facing weight/config surface for tuning without code changes.
