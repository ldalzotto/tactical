const fs = require('node:fs');
const { spawn: defaultSpawn } = require('node:child_process');

const { codeSectionBodyOffset } = require('./wasm-sections');

const SYMBOLIZER_BIN = process.env.LLVM_SYMBOLIZER || 'llvm-symbolizer-22';

function runSymbolizer({ spawn = defaultSpawn, objPath, addresses }) {
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

        proc.stdin.write(addresses.map((addr) => `0x${addr.toString(16)}\n`).join(''));
        proc.stdin.end();
    });
}

// llvm-symbolizer prints one block per input address (blank-line separated).
// Each block is a sequence of (function name, file:line:col) line pairs -
// more than one pair means the frame was inlined at that address.
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

async function symbolicate({ wasmPath, frames, spawn }) {
    const buffer = fs.readFileSync(wasmPath);
    const codeBase = codeSectionBodyOffset(buffer);

    const addresses = frames.map((frame) => frame.offset - codeBase);
    const output = await runSymbolizer({ spawn, objPath: wasmPath, addresses });
    const resolvedLocations = parseSymbolizerOutput(output, frames.length);

    return frames.map((frame, i) => ({
        funcIndex: frame.funcIndex,
        offset: frame.offset,
        locations: addresses[i] < 0 ? [] : resolvedLocations[i],
    }));
}

module.exports = { symbolicate };
