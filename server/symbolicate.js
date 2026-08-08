const fs = require('node:fs');
const { spawn } = require('node:child_process');

const SYMBOLIZER_BIN = 'llvm-symbolizer-22';
const WASM_OBJDUMP_BIN = 'wasm-objdump';

function getWasmCodeOffset({ objPath }) {
    return new Promise((resolve, reject) => {
        const proc = spawn(WASM_OBJDUMP_BIN, ['-h', objPath], {
            stdio: ['pipe', 'pipe', 'pipe'],
        });

        let stdout = '';
        let stderr = '';
        proc.stdout.on('data', (chunk) => { stdout += chunk; });
        proc.stderr.on('data', (chunk) => { stderr += chunk; });

        proc.on('error', reject);
        proc.on('close', (code) => {
            if (code !== 0) {
                reject(new Error(`wasm-objdump exited with code ${code}: ${stderr}`));
                return;
            }
            const codeStart = stdout
                .split('\n')
                .find((line) => line.trimStart().startsWith('Code '));
            const offset = codeStart
                ?.match(/start=(0x[0-9a-fA-F]+)/)?.[1];
            resolve(offset);
        });
    });
}

function runSymbolizer({ objPath, offset0x, addresses }) {
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
        
        const offset = Number.parseInt(offset0x, 16);
        proc.stdin.write(addresses.map((addr) => {
            const address = Number.parseInt(addr, 16);
            const addressWithOffset = address - offset;
            return `0x${addressWithOffset.toString(16)}\n`;
        }).join(''));
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

async function symbolicate({ wasmPath, frames }) {
    const buffer = fs.readFileSync(wasmPath);

    const addresses = frames.map((frame) => frame.offset);
    const offset = await getWasmCodeOffset({ objPath: wasmPath });
    const output = await runSymbolizer({ objPath: wasmPath, offset0x: offset, addresses });
    const resolvedLocations = parseSymbolizerOutput(output, frames.length);

    return frames.map((frame, i) => ({
        funcIndex: frame.funcIndex,
        offset: frame.offset,
        locations: addresses[i] < 0 ? [] : resolvedLocations[i],
    }));
}

module.exports = { symbolicate };
