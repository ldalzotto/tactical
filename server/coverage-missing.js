'use strict';

const fs = require('node:fs');
const path = require('node:path');
const { spawnSync } = require('node:child_process');

// llvm-cov export emits, per file, a sorted list of coverage `segments`:
//   [line, col, count, hasCount, isRegionEntry, isGapRegion]
// A segment with isRegionEntry && hasCount && !isGapRegion starts a real code
// region; its count is how many times that region executed. We turn every
// zero-count region into a clang-style diagnostic.

function expandTabColumn(text, sourceCol) {
    // sourceCol is 1-indexed in the original text (a tab counts as 1 column).
    let visual = 0;
    for (let i = 0; i < sourceCol - 1 && i < text.length; i++) {
        if (text[i] === '\t') {
            visual += 4 - (visual % 4);
        } else {
            visual += 1;
        }
    }
    return visual;
}

function expandTabs(text) {
    let out = '';
    let col = 0;
    for (const ch of text) {
        if (ch === '\t') {
            const spaces = 4 - (col % 4);
            out += ' '.repeat(spaces);
            col += spaces;
        } else {
            out += ch;
            col += 1;
        }
    }
    return out;
}

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
        const uncovered = [];
        for (let i = 0; i < file.segments.length; i++) {
            const [line, col, count, hasCount, isRegionEntry, isGapRegion] = file.segments[i];
            if (!isRegionEntry || !hasCount || isGapRegion || count !== 0) {
                continue;
            }
            const end = file.segments[i + 1] || [line, Infinity, 0, false, false, false];
            uncovered.push({ line, col, end });
        }

        if (uncovered.length === 0) {
            continue;
        }

        let source;
        try {
            source = fs.readFileSync(file.filename, 'utf8').split('\n');
        } catch {
            continue;
        }

        const rel = file.filename.startsWith(rootPrefix)
            ? file.filename.slice(rootPrefix.length)
            : file.filename;

        for (const { line, col, end } of uncovered) {
            const text = source[line - 1] ?? '';
            const display = expandTabs(text);
            const caret = expandTabColumn(text, col);

            let underline = 1;
            if (end[0] === line) {
                const endCaret = expandTabColumn(text, end[1]);
                underline = Math.max(1, endCaret - caret);
            }

            const pad = ' '.repeat(Math.max(0, caret));
            const marker = '^' + '~'.repeat(Math.max(0, underline - 1));

            diagnostics.push(`${rel}:${line}:${col}: error: uncovered code (executed 0 times)`);
            diagnostics.push(`  ${line} | ${display}`);
            diagnostics.push(`    | ${pad}${marker}`);
            total++;
        }
    }

    return { diagnostics, total };
}

module.exports = { printUncovered };
