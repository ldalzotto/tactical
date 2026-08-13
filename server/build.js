const { spawnSync } = require('node:child_process');

const args = process.argv.slice(2);
const mode = args.includes('--release') ? 'Release' : 'Debug';
const verbose = args.includes('--verbose');

// macOS ships Xcode's clang without the WebAssembly backend, so `clang`
// on PATH can't target wasm32. Emscripten bundles a clang build that can;
// prepend it so CMakeLists.txt's `set(CMAKE_C_COMPILER clang)` resolves
// to a wasm32-capable compiler (and wasm-ld, needed for linking).
const EMSCRIPTEN_LLVM_BIN = '/opt/homebrew/opt/emscripten/libexec/llvm/bin';
const env = { ...process.env };
if (require('node:fs').existsSync(EMSCRIPTEN_LLVM_BIN)) {
    env.PATH = `${EMSCRIPTEN_LLVM_BIN}:${env.PATH}`;
}

function run(cmd, cmdArgs) {
    const result = spawnSync(cmd, cmdArgs, { encoding: 'utf8', stdio: verbose ? 'inherit' : 'pipe', env });
    if (result.status !== 0) {
        if (!verbose) {
            process.stdout.write(result.stdout);
            process.stderr.write(result.stderr);
        }
        process.exit(result.status ?? 1);
    }
}

run('cmake', ['-S', '.', '-B', 'build', `-DCMAKE_BUILD_TYPE=${mode}`]);
run('cmake', ['--build', 'build']);
