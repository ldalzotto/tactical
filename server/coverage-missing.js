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

// Regions that are intentionally never executed by the test suite. Keys may
// be `file:line` or `file:line:col`.
const IGNORED_REGIONS = new Set([
    // __builtin_trap: executing it traps the wasm and fails the test run, so
    // no passing test can cover it. This is the trap itself, not a defensive
    // branch that can be converted into an assert_debug invariant.
    'src/lib/assert.c:16:9',
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
