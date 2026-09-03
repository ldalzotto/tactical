const fs = require('node:fs');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const { ensureToolchain } = require('./setup-toolchain');

const verbose = process.argv.includes('--verbose');

ensureToolchain({ verbose });

const BUILD_DIR = 'build';
const COMPILE_COMMANDS = path.join(BUILD_DIR, 'compile_commands.json');

// Must be the same LLVM toolchain used to compile app.wasm (CMakeLists.txt),
// else it parses with a mismatched AST — ensureToolchain() above installs and
// pins a matching version onto PATH when needed.
function findIncludeCleaner() {
    const onPath = spawnSync('which', ['clang-include-cleaner'], { encoding: 'utf8' });
    if (onPath.status !== 0) {
        console.error('clang-include-cleaner not found on PATH');
        process.exit(1);
    }
    return onPath.stdout.trim();
}

const compileCommands = JSON.parse(fs.readFileSync(COMPILE_COMMANDS, 'utf8'));
const files = compileCommands.map((entry) => entry.file);

const includeCleaner = findIncludeCleaner();
const result = spawnSync(includeCleaner, ['-p', BUILD_DIR, '--edit', ...files], { encoding: 'utf8', stdio: verbose ? 'inherit' : 'pipe' });
if (result.status !== 0 && !verbose) {
    process.stdout.write(result.stdout);
    process.stderr.write(result.stderr);
}
process.exit(result.status ?? 1);
