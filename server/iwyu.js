const fs = require('node:fs');
const path = require('node:path');
const { spawnSync } = require('node:child_process');

const verbose = process.argv.includes('--verbose');

const BUILD_DIR = 'build';
const COMPILE_COMMANDS = path.join(BUILD_DIR, 'compile_commands.json');

// clang-include-cleaner must come from the same LLVM toolchain as the
// clang used to compile app.wasm (see CMakeLists.txt) -- otherwise it
// parses with a mismatched AST. Prefer PATH so a machine that symlinks it
// there just works, but fall back to the versioned path this repo was
// verified against.
function findIncludeCleaner() {
    const onPath = spawnSync('which', ['clang-include-cleaner'], { encoding: 'utf8' });
    if (onPath.status === 0) {
        return onPath.stdout.trim();
    }
    return '/usr/lib/llvm-22/bin/clang-include-cleaner';
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
