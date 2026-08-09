const fs = require('node:fs');
const path = require('node:path');

const { symbolicate } = require('./symbolicate');

const ROOT = path.join(__dirname, '..');
const WASM_PATH = path.join(ROOT, 'build', 'app.wasm');
const MEMORY_PAGES = 32;

function decodeWasmString(memory, ptr, len) {
    const bytes = new Uint8Array(memory.buffer, ptr, len);
    return new TextDecoder().decode(bytes);
}

function parseTrapFrames(stack) {
    const re = /wasm-function\[(\d+)\]:(0x[0-9a-fA-F]+)/g;
    const frames = [];
    let match;
    while ((match = re.exec(stack)) !== null) {
        frames.push({ funcIndex: Number(match[1]), offset: match[2] });
    }
    return frames;
}

function formatResolvedFrame(frame) {
    if (!frame.locations || frame.locations.length === 0) {
        return `  wasm-function[${frame.funcIndex}]:${frame.offset} (unresolved)`;
    }
    return frame.locations
        .map((loc) => `  ${loc.function} at ${loc.file}:${loc.line}:${loc.column}`)
        .join('\n');
}

async function describeFailure(err) {
    const message = err.message || String(err);
    const rawFrames = parseTrapFrames(err.stack || '');
    if (rawFrames.length === 0) {
        return message;
    }
    try {
        const resolvedFrames = await symbolicate({ wasmPath: WASM_PATH, frames: rawFrames });
        return `${message}\n${resolvedFrames.map(formatResolvedFrame).join('\n')}`;
    } catch (symbolicateErr) {
        return `${message}\n(symbolication failed: ${symbolicateErr.message})`;
    }
}

async function main() {
    const memory = new WebAssembly.Memory({ initial: MEMORY_PAGES });
    const importObject = {
        env: {
            memory,
            create_window() { return 0; },
            present_window() {},
            debug_log(beginPtr, endPtr) {
                console.log(decodeWasmString(memory, beginPtr, endPtr - beginPtr));
            },
        },
    };

    const wasmBuffer = fs.readFileSync(WASM_PATH);
    const { instance } = await WebAssembly.instantiate(wasmBuffer, importObject);
    const { test_discovery_count, test_discovery_name_begin, test_discovery_name_end, test_discovery_fn_at, test_run } = instance.exports;
    const count = test_discovery_count();

    let passed = 0;
    let failed = 0;

    for (let i = 0; i < count; i++) {
        const beginPtr = test_discovery_name_begin(i);
        const endPtr = test_discovery_name_end(i);
        const name = decodeWasmString(memory, beginPtr, endPtr - beginPtr);
        const fn = test_discovery_fn_at(i);

        try {
            test_run(fn);
            passed++;
        } catch (err) {
            failed++;
            console.log(`FAIL  ${name}`);
            const detail = await describeFailure(err);
            console.log(detail.replace(/^/gm, '      '));
        }
    }

    console.log(`\nPassed ${passed}/${count}, Failed ${failed}/${count}`);
    process.exitCode = failed > 0 ? 1 : 0;
}

main().catch((err) => {
    console.error(err);
    process.exitCode = 1;
});
