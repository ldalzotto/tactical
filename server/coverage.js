'use strict';

const fs = require('node:fs');
const path = require('node:path');
const { spawnSync } = require('node:child_process');

const { symbolicate } = require('./symbolicate');
const { runWasmTests } = require('../web/wasm-shared');
const { buildTextProfile } = require('./wasm-profile');
const { printUncovered } = require('./coverage-missing');

const ROOT = path.join(__dirname, '..');
const WASM_PATH = path.join(ROOT, 'build', 'app.wasm');
const PROF_TEXT = path.join(ROOT, 'build', 'coverage.proftext');
const PROF_DATA = path.join(ROOT, 'build', 'coverage.profdata');
const HTML_DIR = path.join(ROOT, 'build', 'coverage-html');

function run(cmd, args) {
    const result = spawnSync(cmd, args, { encoding: 'utf8' });
    if (result.status !== 0) {
        process.stdout.write(result.stdout);
        process.stderr.write(result.stderr);
        process.exit(result.status ?? 1);
    }
    return result;
}

async function main() {
    const wasmBytes = fs.readFileSync(WASM_PATH);
    const resolveFrames = (frames) => symbolicate({ wasmPath: WASM_PATH, frames });

    const { failed, memory } = await runWasmTests({
        wasmBytes,
        resolveFrames,
        onResult({ name, passed, detail }) {
            if (!passed) {
                console.log(`FAIL  ${name}`);
                console.log(detail.replace(/^/gm, '      '));
            }
        },
        onComplete({ passed, failed, count }) {
            console.log(`\nTests: ${passed}/${count} passed, ${failed} failed`);
        },
    });

    if (failed > 0) {
        process.exitCode = 1;
        return;
    }

    const proftext = buildTextProfile({ wasmBytes, memory });
    fs.writeFileSync(PROF_TEXT, proftext);

    run('llvm-profdata', ['merge', '-o', PROF_DATA, PROF_TEXT]);

    // Keep the annotated HTML report scoped to the same game.h-reachable
    // sources as printUncovered (see coverage-missing.js), so the browser
    // view doesn't show 0% for the wasm entry point, the renderer, the input
    // poller, the scenario builder, the frame clock, the runtime/graphics
    // wrappers or the test harness itself.
    run('llvm-cov', [
        'show',
        WASM_PATH,
        `-instr-profile=${PROF_DATA}`,
        '-format=html',
        `-output-dir=${HTML_DIR}`,
        '--ignore-filename-regex=(^|/)(main|render|input|scenario|clock|graphics|runtime|coverage_stub)\\.c$',
        '--ignore-filename-regex=(^|/)test[^/]*\\.(c|h)$',
    ]);
    console.log(`\nAnnotated line coverage: build/coverage-html/index.html`);

    const { diagnostics, total } = printUncovered({
        wasmPath: WASM_PATH,
        profDataPath: PROF_DATA,
        root: ROOT,
    });
    if (total > 0) {
        console.log(`\n=== Uncovered code (${total} regions) ===\n`);
        process.stdout.write(diagnostics.join('\n') + '\n');
    }

    process.exitCode = 0;
}

main().catch((err) => {
    console.error(err);
    process.exitCode = 1;
});
