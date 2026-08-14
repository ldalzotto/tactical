'use strict';

const path = require('node:path');
const { spawnSync } = require('node:child_process');

// llvm-cov export emits, per file, a sorted list of coverage `segments`:
//   [line, col, count, hasCount, isRegionEntry, isGapRegion]
// A segment with isRegionEntry && hasCount && !isGapRegion starts a real code
// region; its count is how many times that region executed. We turn every
// zero-count region into a single, agent-friendly `file:line:col` diagnostic.

// The tests may only drive the game through src/game/game.h. That means the
// only code that can legitimately be covered is the transitive closure of
// game.h: game.c itself, the modules it calls, and the lib modules those
// depend on. Everything else (the wasm entry point, the renderer, the input
// poller, the scenario builder, the frame clock, the graphics/runtime wrappers
// and the test harness itself) is either not reachable from game.h or is test
// scaffolding, so reporting uncovered regions there would be noise.
const INCLUDED_FILES = new Set([
    'src/game/game.c',
    'src/game/action.c',
    'src/game/ai.c',
    'src/game/pathing.c',
    'src/game/skill.c',
    'src/game/render_cache.c',
    'src/game/entity.c',
    'src/game/grid.c',
    'src/game/layout.c',
    'src/game/ui.c',
    'src/game/position.c',
    'src/game/turn.c',
    'src/lib/memory.c',
    'src/lib/assert.c',
]);

// Regions that are intentionally never executed by the test suite. These are
// defensive traps/branches that cannot be reached through the game.h API
// without either crashing the wasm runner or bypassing a guard the public API
// already enforces. Keys may be `file:line` or `file:line:col`.
const IGNORED_REGIONS = new Set([
    // __builtin_trap: reaching it would abort the test run.
    'src/lib/assert.c:16:9',

    // action_try_attack's dead-defender guard. game.h never routes a click to
    // a dead defender (entity_find_at only returns alive entities), so the
    // right side of `!attacker->alive || !defender->alive` is unreachable
    // from the public game API. The guard is kept as a documented contract of
    // action.h.
    'src/game/action.c:24:47',
    'src/game/action.c:25:16',

    // ai_step_toward's `!found` guard. ai_run_ennemy_turn only calls
    // ai_step_toward after ai_find_nearest_player found a reachable player,
    // which implies at least one of the enemy's neighbors is reachable from
    // that player (the BFS path is reversible), so `found` is always true.
    // The guard is defensive and cannot be reached through game.h.
    'src/game/ai.c:93:17',
    'src/game/ai.c:94:16',
    'src/game/ai.c:153:72',
]);

function run(cmd, args) {
    const result = spawnSync(cmd, args, { encoding: 'utf8' });
    if (result.status !== 0) {
        process.stdout.write(result.stdout);
        process.stderr.write(result.stderr);
        process.exit(result.status ?? 1);
    }
    return result.stdout;
}

function printUncovered({ wasmPath, profDataPath, root }) {
    const json = run('llvm-cov', ['export', wasmPath, `-instr-profile=${profDataPath}`]);
    const data = JSON.parse(json);

    const rootPrefix = root + path.sep;
    const diagnostics = [];
    let total = 0;

    for (const file of data.data[0].files) {
        const rel = file.filename.startsWith(rootPrefix)
            ? file.filename.slice(rootPrefix.length)
            : file.filename;

        if (!INCLUDED_FILES.has(rel)) {
            continue;
        }

        for (const [line, col, count, hasCount, isRegionEntry, isGapRegion] of file.segments) {
            if (!isRegionEntry || !hasCount || isGapRegion || count !== 0) {
                continue;
            }
            if (IGNORED_REGIONS.has(`${rel}:${line}`) || IGNORED_REGIONS.has(`${rel}:${line}:${col}`)) {
                continue;
            }
            diagnostics.push(`${rel}:${line}:${col}: error: uncovered code (executed 0 times)`);
            total++;
        }
    }

    return { diagnostics, total };
}

module.exports = { printUncovered };
