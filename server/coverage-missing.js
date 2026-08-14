'use strict';

const path = require('node:path');
const { spawnSync } = require('node:child_process');

// llvm-cov export emits, per file, a sorted list of coverage `segments`:
//   [line, col, count, hasCount, isRegionEntry, isGapRegion]
// A segment with isRegionEntry && hasCount && !isGapRegion starts a real code
// region; its count is how many times that region executed. We turn every
// zero-count region into a single, agent-friendly `file:line:col` diagnostic.

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

        for (const [line, col, count, hasCount, isRegionEntry, isGapRegion] of file.segments) {
            if (!isRegionEntry || !hasCount || isGapRegion || count !== 0) {
                continue;
            }
            diagnostics.push(`${rel}:${line}:${col}: error: uncovered code (executed 0 times)`);
            total++;
        }
    }

    return { diagnostics, total };
}

module.exports = { printUncovered };
