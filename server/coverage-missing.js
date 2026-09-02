'use strict';

const path = require('node:path');
const { spawnSync } = require('node:child_process');

// llvm-cov export emits, per file, a `summary` with per-metric totals
// ({ count, covered, notcovered, percent }) plus detailed arrays that back
// it: `segments`, `branches`, `expansions`. The summary is authoritative for
// "is anything missing" (it's what the HTML report renders); the detailed
// arrays are used only to attach a `file:line:col` to each gap.
//
// Branch entry: [line, col, lineEnd, colEnd, trueCount, falseCount, ...]. A
// side with count 0 is a gap even if the line itself executed.

function run(cmd, args) {
    const result = spawnSync(cmd, args, { encoding: 'utf8', maxBuffer: Infinity });
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

    for (const file of data.data[0].files) {
        const rel = relName(file.filename);

        // Gaps attributed below; the summary net reports what's left unlocalized.
        const found = { regions: 0, branches: 0, functions: 0, instantiations: 0, lines: 0, mcdc: 0 };

        // 1. Uncovered regions (executed 0 times). Only top-level segments
        // carry region-entry flags; macro-body regions go to the summary net.
        for (const [line, col, count, hasCount, isRegionEntry, isGapRegion] of file.segments || []) {
            if (!isRegionEntry || !hasCount || isGapRegion || count !== 0) {
                continue;
            }
            found.regions++;
            push(rel, line, col, 'uncovered code (executed 0 times)');
        }

        // 2. Uncovered branches. Top-level branches report at their own
        // location; expansion branches report at the macro invocation site
        // (the source the developer can actually edit).
        const scanBranches = (branches, line, col) => {
            let missing = 0;
            for (const branch of branches || []) {
                const locLine = line ?? branch[0];
                const locCol = col ?? branch[1];
                if (branch[4] === 0) {
                    missing++;
                    push(rel, locLine, locCol, 'uncovered branch (condition is never true)');
                }
                if (branch[5] === 0) {
                    missing++;
                    push(rel, locLine, locCol, 'uncovered branch (condition is never false)');
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

        // 3. Summary net: surfaces anything the passes above couldn't
        // attribute (macro-body regions, function/line gaps, mcdc), so no
        // gap class is silently dropped.
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
