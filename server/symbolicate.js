const { spawn } = require('node:child_process');

const SYMBOLIZER_BIN = 'llvm-symbolizer';
const WASM_OBJDUMP_BIN = 'wasm-objdump';

function run(cmd, args) {
    return new Promise((resolve, reject) => {
        const proc = spawn(cmd, args, { stdio: ['pipe', 'pipe', 'pipe'] });

        let stdout = '';
        let stderr = '';
        proc.stdout.on('data', (chunk) => { stdout += chunk; });
        proc.stderr.on('data', (chunk) => { stderr += chunk; });

        proc.on('error', reject);
        proc.on('close', (code) => {
            if (code !== 0) {
                reject(new Error(`${cmd} exited with code ${code}: ${stderr}`));
                return;
            }
            resolve(stdout);
        });
    });
}

// Ground truth for calibration: funcIndex -> declared name, read from the
// wasm module directly (independent of DWARF/symbolizer conventions).
async function getFuncNames({ objPath }) {
    const stdout = await run(WASM_OBJDUMP_BIN, ['-x', objPath]);
    const names = new Map();
    const re = /^\s*-\s*func\[(\d+)\].*<([^>]+)>/;
    for (const line of stdout.split('\n')) {
        const match = re.exec(line);
        if (match) {
            names.set(Number(match[1]), match[2]);
        }
    }
    return names;
}

async function getWasmCodeOffset({ objPath }) {
    const stdout = await run(WASM_OBJDUMP_BIN, ['-h', objPath]);
    const codeStart = stdout
        .split('\n')
        .find((line) => line.trimStart().startsWith('Code '));
    return codeStart?.match(/start=(0x[0-9a-fA-F]+)/)?.[1];
}

function runSymbolizer({ objPath, addresses }) {
    return new Promise((resolve, reject) => {
        const proc = spawn(SYMBOLIZER_BIN, ['--obj=' + objPath, '-f', '-C', '--inlines'], {
            stdio: ['pipe', 'pipe', 'pipe'],
        });

        let stdout = '';
        let stderr = '';
        proc.stdout.on('data', (chunk) => { stdout += chunk; });
        proc.stderr.on('data', (chunk) => { stderr += chunk; });

        proc.on('error', reject);
        proc.on('close', (code) => {
            if (code !== 0) {
                reject(new Error(`llvm-symbolizer exited with code ${code}: ${stderr}`));
                return;
            }
            resolve(stdout);
        });

        proc.stdin.write(addresses.map((addr) => `${addr}\n`).join(''));
        proc.stdin.end();
    });
}

// llvm-symbolizer prints one blank-line-separated block per input address, as
// (function name, file:line:col) pairs — more than one pair means inlining.
function parseSymbolizerOutput(output, count) {
    const blocks = output.split(/\n\n+/).filter((b) => b.trim().length > 0);

    return Array.from({ length: count }, (_, i) => {
        const block = blocks[i] || '';
        const lines = block.split('\n').filter((l) => l.length > 0);
        const locations = [];

        for (let i2 = 0; i2 < lines.length; i2 += 2) {
            const functionName = lines[i2];
            const locationLine = lines[i2 + 1] || '??:0:0';
            const match = /^(.*):(\d+):(\d+)$/.exec(locationLine);

            locations.push({
                function: functionName,
                file: match ? match[1] : locationLine,
                line: match ? Number(match[2]) : 0,
                column: match ? Number(match[3]) : 0,
            });
        }

        return locations;
    });
}

function locationsMatchName(locations, expectedName) {
    return locations.some((loc) => loc.function === expectedName);
}

// LLVM/Emscripten versions disagree on whether DWARF addresses are absolute
// file offsets (matching V8's trap stack traces) or relative to the Code
// section start. Calibrate per binary: resolve one frame both ways and see
// which matches the name we already know from the function index.
async function needsCodeOffsetSubtraction({ objPath, frame, funcNames }) {
    const expectedName = funcNames.get(frame.funcIndex);
    if (!expectedName) {
        return false;
    }

    const rawOutput = await runSymbolizer({ objPath, addresses: [frame.offset] });
    const [rawLocations] = parseSymbolizerOutput(rawOutput, 1);
    if (locationsMatchName(rawLocations, expectedName)) {
        return false;
    }

    const codeOffset = await getWasmCodeOffset({ objPath });
    if (!codeOffset) {
        return false;
    }
    const adjusted = `0x${(Number.parseInt(frame.offset, 16) - Number.parseInt(codeOffset, 16)).toString(16)}`;
    const adjustedOutput = await runSymbolizer({ objPath, addresses: [adjusted] });
    const [adjustedLocations] = parseSymbolizerOutput(adjustedOutput, 1);
    return locationsMatchName(adjustedLocations, expectedName);
}

// Calibration result and the func-name table are properties of the
// (toolchain, wasm binary) pair, not of any particular run, so both are
// cheap and safe to cache per object path.
const calibrationCache = new Map();
const funcNamesCache = new Map();

async function getCachedFuncNames({ objPath }) {
    let funcNames = funcNamesCache.get(objPath);
    if (!funcNames) {
        funcNames = await getFuncNames({ objPath });
        funcNamesCache.set(objPath, funcNames);
    }
    return funcNames;
}

async function symbolicate({ wasmPath, frames }) {
    if (frames.length === 0) {
        return [];
    }

    const funcNames = await getCachedFuncNames({ objPath: wasmPath });

    let subtractOffset = calibrationCache.get(wasmPath);
    if (subtractOffset === undefined) {
        subtractOffset = await needsCodeOffsetSubtraction({ objPath: wasmPath, frame: frames[0], funcNames });
        calibrationCache.set(wasmPath, subtractOffset);
    }

    const codeOffset = subtractOffset ? await getWasmCodeOffset({ objPath: wasmPath }) : undefined;
    const codeOffsetNum = codeOffset ? Number.parseInt(codeOffset, 16) : 0;

    const addresses = frames.map((frame) => {
        if (!subtractOffset) {
            return frame.offset;
        }
        return `0x${(Number.parseInt(frame.offset, 16) - codeOffsetNum).toString(16)}`;
    });

    const output = await runSymbolizer({ objPath: wasmPath, addresses });
    const resolvedLocations = parseSymbolizerOutput(output, frames.length);

    return frames.map((frame, i) => ({
        funcIndex: frame.funcIndex,
        offset: frame.offset,
        locations: patchOutermostFunctionName(resolvedLocations[i], funcNames.get(frame.funcIndex)),
    }));
}

// llvm-symbolizer's function-name resolution for wasm/DWARF has been
// observed to misattribute the outermost (non-inlined) frame to an
// unrelated function's name while still resolving that same frame's
// file:line correctly (verified against the DWARF subprogram's own
// low_pc). The wasm binary's name section, looked up by funcIndex, is
// unambiguous ground truth for that outermost frame, so it overrides
// whatever name llvm-symbolizer reported there. Inlined frames (every
// location before the last) aren't touched: they describe logical
// functions with no funcIndex of their own.
function patchOutermostFunctionName(locations, groundTruthName) {
    if (!groundTruthName || locations.length === 0) {
        return locations;
    }
    const outermost = locations.length - 1;
    return locations.map((loc, i) => (i === outermost ? { ...loc, function: groundTruthName } : loc));
}

// panic_without_expect_panic_traps (src/test_runtime.c) deterministically
// traps via panic(false), giving runWasmTests a real wasm stack to
// symbolicate every run. Checking its resolved detail here is what catches
// symbolication silently degrading -- e.g. wasm-objdump/llvm-symbolizer
// missing from the toolchain -- instead of it only showing up as garbled
// text in an unrelated test failure.
const SYMBOLICATION_CHECK_TEST_NAME = 'panic_without_expect_panic_traps';
// panic(false) is written on this exact line of test_runtime.c, inside the
// function that must show up as the resolved trace's outermost frame.
// patchOutermostFunctionName takes that frame's function name from the wasm
// binary's name section (via funcIndex) rather than from llvm-symbolizer's
// own DWARF subprogram lookup, which has been observed to misattribute it to
// an unrelated function even when the file:line it reports is correct.
const SYMBOLICATION_CHECK_EXPECTED_FUNCTION = 'test_panic_without_expect_panic_traps';
const SYMBOLICATION_CHECK_EXPECTED_FILE = 'test_runtime.c';
const SYMBOLICATION_CHECK_EXPECTED_LINE = ':45:';

// Called from a runWasmTests onResult callback for every test. Returns an
// error string if the named test's resolved stack trace doesn't look right,
// or null if there's nothing to check (wrong test, or nothing went wrong).
function checkSymbolicationDetail(name, detail) {
    if (name !== SYMBOLICATION_CHECK_TEST_NAME) {
        return null;
    }
    if (!detail || detail.includes('symbolication failed')) {
        return `expected a resolved stack trace for '${SYMBOLICATION_CHECK_TEST_NAME}', got: ${detail}`;
    }
    if (
        !detail.includes(SYMBOLICATION_CHECK_EXPECTED_FUNCTION) ||
        !detail.includes(SYMBOLICATION_CHECK_EXPECTED_FILE) ||
        !detail.includes(SYMBOLICATION_CHECK_EXPECTED_LINE)
    ) {
        return `expected the stack trace for '${SYMBOLICATION_CHECK_TEST_NAME}' to include ${SYMBOLICATION_CHECK_EXPECTED_FUNCTION} at ${SYMBOLICATION_CHECK_EXPECTED_FILE}${SYMBOLICATION_CHECK_EXPECTED_LINE}, got:\n${detail}`;
    }
    return null;
}

module.exports = { symbolicate, checkSymbolicationDetail };
