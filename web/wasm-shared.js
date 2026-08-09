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

async function resolveFailureText(err, resolveFrames) {
    const message = err.message || String(err);
    const rawFrames = parseTrapFrames(err.stack || '');
    if (rawFrames.length === 0) {
        return { message, framesText: '' };
    }
    try {
        const resolvedFrames = await resolveFrames(rawFrames);
        return { message, framesText: resolvedFrames.map(formatResolvedFrame).join('\n') };
    } catch (symbolicateErr) {
        return { message, framesText: `(symbolication failed: ${symbolicateErr.message})` };
    }
}

function decodeWasmMemoryString(memory, ptr, len) {
    const bytes = new Uint8Array(memory.buffer, ptr, len);
    return new TextDecoder().decode(bytes);
}

async function runWasmTests({ wasmBytes, importObject, resolveFrames, onResult, onComplete }) {
    const { instance } = await WebAssembly.instantiate(wasmBytes, importObject);
    const memory = importObject.env.memory;
    const { test_discovery_count, test_discovery_name_begin, test_discovery_name_end, test_discovery_fn_at, test_run } = instance.exports;
    const count = test_discovery_count();

    let passed = 0;
    let failed = 0;

    for (let i = 0; i < count; i++) {
        const beginPtr = test_discovery_name_begin(i);
        const endPtr = test_discovery_name_end(i);
        const name = decodeWasmMemoryString(memory, beginPtr, endPtr - beginPtr);
        const fn = test_discovery_fn_at(i);

        try {
            test_run(fn);
            passed++;
            onResult({ name, passed: true });
        } catch (err) {
            failed++;
            const { message, framesText } = await resolveFailureText(err, resolveFrames);
            const detail = framesText ? `${message}\n${framesText}` : message;
            onResult({ name, passed: false, detail });
        }
    }

    onComplete({ passed, failed, count });
    return { passed, failed, count };
}

if (typeof module !== 'undefined') {
    module.exports = { parseTrapFrames, formatResolvedFrame, resolveFailureText, decodeWasmMemoryString, runWasmTests };
}
