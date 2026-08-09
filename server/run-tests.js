const fs = require('node:fs');
const path = require('node:path');

const { symbolicate } = require('./symbolicate');
const { runWasmTests } = require('../web/wasm-shared');

const ROOT = path.join(__dirname, '..');
const WASM_PATH = path.join(ROOT, 'build', 'app.wasm');
const MEMORY_PAGES = 32;

async function main() {
    const memory = new WebAssembly.Memory({ initial: MEMORY_PAGES });
    const importObject = {
        env: {
            memory,
            create_window() { return 0; },
            present_window() {},
            debug_log(beginPtr, endPtr) {
                const bytes = new Uint8Array(memory.buffer, beginPtr, endPtr - beginPtr);
                console.log(new TextDecoder().decode(bytes));
            },
        },
    };

    const wasmBytes = fs.readFileSync(WASM_PATH);
    const resolveFrames = (frames) => symbolicate({ wasmPath: WASM_PATH, frames });

    await runWasmTests({
        wasmBytes,
        importObject,
        resolveFrames,
        onResult({ name, passed, detail }) {
            if (!passed) {
                console.log(`FAIL  ${name}`);
                console.log(detail.replace(/^/gm, '      '));
            }
        },
        onComplete({ passed, failed, count }) {
            console.log(`\nPassed ${passed}/${count}, Failed ${failed}/${count}`);
            process.exitCode = failed > 0 ? 1 : 0;
        },
    });
}

main().catch((err) => {
    console.error(err);
    process.exitCode = 1;
});
