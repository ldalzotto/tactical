const fs = require('node:fs');
const path = require('node:path');

const { symbolicate, checkSymbolicationDetail } = require('./symbolicate');
const { runWasmTests } = require('../web/wasm-shared');

const ROOT = path.join(__dirname, '..');
const WASM_PATH = path.join(ROOT, 'build', 'app.wasm');
const verbose = process.argv.includes('--verbose');

async function main() {
    const wasmBytes = fs.readFileSync(WASM_PATH);
    const resolveFrames = (frames) => symbolicate({ wasmPath: WASM_PATH, frames });
    let symbolicationError = null;

    await runWasmTests({
        wasmBytes,
        resolveFrames,
        printLine: verbose ? console.log : () => {},
        onResult({ name, passed, detail }) {
            symbolicationError ??= checkSymbolicationDetail(name, detail);
            if (!passed) {
                console.log(`FAIL  ${name}`);
                console.log(detail.replace(/^/gm, '      '));
            }
        },
        onComplete({ passed, failed, count }) {
            console.log(`\nPassed ${passed}/${count}, Failed ${failed}/${count}`);
            if (symbolicationError) {
                console.log(`\nFAIL  symbolication check: ${symbolicationError}`);
            }
            process.exitCode = failed > 0 || symbolicationError ? 1 : 0;
        },
    });
}

main().catch((err) => {
    console.error(err);
    process.exitCode = 1;
});
