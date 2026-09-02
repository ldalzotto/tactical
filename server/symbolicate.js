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

// Different LLVM/Emscripten versions have disagreed on whether the DWARF
// addresses embedded in a wasm module's debug info are absolute file offsets
// (matching the offsets V8 reports in trap stack traces) or relative to the
// start of the Code section. Rather than assume one convention, calibrate
// against this specific binary: resolve one frame both ways and see which
// one actually names the function we know (from the module's own function
// index) it should be.
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

// Calibration result is a property of the (toolchain, wasm binary) pair, not
// of any particular run, so it's cheap and safe to cache per object path.
const calibrationCache = new Map();

async function symbolicate({ wasmPath, frames }) {
    if (frames.length === 0) {
        return [];
    }

    let subtractOffset = calibrationCache.get(wasmPath);
    if (subtractOffset === undefined) {
        const funcNames = await getFuncNames({ objPath: wasmPath });
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
        locations: resolvedLocations[i],
    }));
}

module.exports = { symbolicate };
