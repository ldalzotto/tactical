'use strict';

const path = require('node:path');
const { spawnSync } = require('node:child_process');

// llvm-cov export emits, per file, a `summary` with per-metric totals
// ({ count, covered, notcovered, percent }) plus the detailed arrays that
// back it: `segments`, `branches` and `expansions`.
//
// The summary is what `llvm-cov show` / the annotated HTML report renders, so
// it is the authoritative "is anything missing" signal. We use the detailed
// arrays only to attach a `file:line:col` location to each gap.
//
// A branch entry is [line, col, lineEnd, colEnd, trueCount, falseCount, ...].
// A side whose count is 0 is a coverage gap even though the line itself may
// have executed — this is exactly the "missing branch" the HTML report flags.

// Regions that are intentionally never executed by the test suite. Keys may
// be `file:line` or `file:line:col`.
const IGNORED_REGIONS = new Set([
    // __builtin_trap: executing it traps the wasm and fails the test run, so
    // no passing test can cover it. This is the trap itself, not a defensive
    // branch that can be converted into an assert_debug invariant.
    'src/lib/assert.c:16:9',
    // panic's "no expect_panic in effect" branch falls through to the trap
    // above, so covering it would trap the wasm and fail the test run. The
    // expect_panic side is covered by the negative-assertion tests; only the
    // trapping side is unreachable from a passing suite.
    'src/lib/assert.c:11:13',
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

    const relName = (filename) =>
        filename.startsWith(rootPrefix) ? filename.slice(rootPrefix.length) : filename;

    const push = (rel, line, col, msg) => {
        const where = line ? `${rel}:${line}:${col}` : rel;
        diagnostics.push(`${where}: error: ${msg}`);
        total++;
    };

    const isIgnored = (rel, line, col) =>
        IGNORED_REGIONS.has(`${rel}:${line}`) || IGNORED_REGIONS.has(`${rel}:${line}:${col}`);

    for (const file of data.data[0].files) {
        const rel = relName(file.filename);

        // Gaps attributed by the detailed passes below; the summary net
        // reports only what these passes could not localize.
        const found = { regions: 0, branches: 0, functions: 0, instantiations: 0, lines: 0, mcdc: 0 };

        // 1. Uncovered regions (statements executed 0 times). Only top-level
        // segments carry region-entry flags; macro-body regions are handled
        // by the summary net below.
        for (const [line, col, count, hasCount, isRegionEntry, isGapRegion] of file.segments || []) {
            if (!isRegionEntry || !hasCount || isGapRegion || count !== 0) {
                continue;
            }
            found.regions++;
            if (isIgnored(rel, line, col)) {
                continue;
            }
            push(rel, line, col, 'uncovered code (executed 0 times)');
        }

        // 2. Uncovered branches (a side of a condition that never executed).
        // Top-level branches report at their own location; expansion branches
        // report at the macro invocation site, which is where the source code
        // the developer can change actually lives.
        const scanBranches = (branches, line, col) => {
            let missing = 0;
            for (const branch of branches || []) {
                const locLine = line ?? branch[0];
                const locCol = col ?? branch[1];
                if (branch[4] === 0) {
                    missing++;
                    if (!isIgnored(rel, locLine, locCol)) {
                        push(rel, locLine, locCol, 'uncovered branch (condition is never true)');
                    }
                }
                if (branch[5] === 0) {
                    missing++;
                    if (!isIgnored(rel, locLine, locCol)) {
                        push(rel, locLine, locCol, 'uncovered branch (condition is never false)');
                    }
                }
            }
            return missing;
        };

        found.branches += scanBranches(file.branches);

        const walkExpansions = (expansions) => {
            for (const exp of expansions || []) {
                const [line, col] = exp.source_region || [];
                found.branches += scanBranches(exp.branches, line, col);
                walkExpansions(exp.expansions);
            }
        };
        walkExpansions(file.expansions);

        // 3. Summary net. llvm-cov already totals every metric; anything the
        // passes above could not attribute (macro-body regions, function or
        // line gaps, future mcdc instrumentation) is surfaced here so no
        // class of gap is silently dropped.
        const summary = file.summary || {};
        for (const metric of ['regions', 'branches', 'functions', 'instantiations', 'lines', 'mcdc']) {
            const m = summary[metric];
            const notcovered = (m && m.notcovered) || 0;
            if (notcovered > (found[metric] || 0)) {
                push(
                    rel,
                    0,
                    0,
                    `${metric} coverage incomplete: ${m.covered}/${m.count} covered ` +
                        `(${notcovered} not covered; see build/coverage-html/index.html for locations)`,
                );
            }
        }
    }

    return { diagnostics, total };
}

module.exports = { printUncovered };
