'use strict';

// Ensures the native toolchain (cmake, clang, lld, and matching
// llvm-cov/llvm-profdata) is available before build.js/coverage.js shell
// out to it. Installs missing pieces via apt when possible (CI runners),
// and is a fast no-op when a working, version-consistent toolchain is
// already on PATH (e.g. this sandbox, or a dev machine).

const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');

const REQUIRED_LLVM_MAJOR = '22';
const LLVM_BIN_DIR = path.join(os.homedir(), '.cache', 'app-llvm22-bin');

function run(cmd, args, opts = {}) {
    return spawnSync(cmd, args, { encoding: 'utf8', ...opts });
}

function commandExists(cmd) {
    return run('which', [cmd]).status === 0;
}

function majorVersion(cmd) {
    const result = run(cmd, ['--version']);
    if (result.status !== 0) return null;
    const match = (result.stdout || '').match(/version (\d+)/i);
    return match ? match[1] : null;
}

function apt(args) {
    return run('sudo', ['apt-get', ...args], { stdio: 'inherit' });
}

function ensureCmake() {
    if (commandExists('cmake')) return;
    console.log('[setup-toolchain] cmake not found, installing...');
    apt(['update']);
    if (apt(['install', '-y', 'cmake']).status !== 0) {
        console.error('[setup-toolchain] Failed to install cmake. Install it manually and retry.');
        process.exit(1);
    }
}

function ensureClangAndLld() {
    if (commandExists('clang') && commandExists('lld')) return;
    console.log('[setup-toolchain] clang/lld not found, installing...');
    apt(['update']);
    if (apt(['install', '-y', 'clang', 'lld']).status !== 0) {
        console.error('[setup-toolchain] Failed to install clang/lld. Install them manually and retry.');
        process.exit(1);
    }
}

// build.js compiles with `clang` and coverage.js reads the resulting
// profile with `llvm-cov`/`llvm-profdata`; a version mismatch between them
// makes coverage.js report "no coverage data found". Only reach for a
// specific LLVM install when the toolchain already on PATH isn't
// self-consistent.
function llvmToolchainConsistent() {
    if (!commandExists('clang') || !commandExists('llvm-cov') || !commandExists('llvm-profdata')) return false;
    const clangVer = majorVersion('clang');
    const covVer = majorVersion('llvm-cov');
    const profVer = majorVersion('llvm-profdata');
    return clangVer === REQUIRED_LLVM_MAJOR && clangVer === covVer && clangVer === profVer;
}

function installLlvm(major) {
    console.log(`[setup-toolchain] Installing LLVM ${major}...`);
    const scriptPath = path.join(os.tmpdir(), 'llvm.sh');
    if (run('wget', ['-O', scriptPath, 'https://apt.llvm.org/llvm.sh'], { stdio: 'inherit' }).status !== 0) {
        console.error('[setup-toolchain] Failed to download the LLVM install script.');
        process.exit(1);
    }
    fs.chmodSync(scriptPath, 0o755);
    const install = run('sudo', [scriptPath, major, 'all'], { stdio: 'inherit' });
    fs.rmSync(scriptPath, { force: true });
    if (install.status !== 0) {
        console.error(`[setup-toolchain] Failed to install LLVM ${major}. Install it manually and retry.`);
        process.exit(1);
    }
}

// Symlinks the versioned LLVM tool names (clang-22, llvm-cov-22, ...) to
// their unversioned names in a dedicated directory, then prepends that
// directory to PATH so cmake/build.js/coverage.js resolve the pinned
// version instead of whatever else update-alternatives has registered.
function linkVersionedTools(major) {
    const names = ['clang', 'llvm-cov', 'llvm-profdata'];
    fs.mkdirSync(LLVM_BIN_DIR, { recursive: true });
    for (const name of names) {
        const versioned = `/usr/bin/${name}-${major}`;
        if (!fs.existsSync(versioned)) {
            console.error(`[setup-toolchain] Expected ${versioned} after installing LLVM ${major}.`);
            process.exit(1);
        }
        const dest = path.join(LLVM_BIN_DIR, name);
        fs.rmSync(dest, { force: true });
        fs.symlinkSync(versioned, dest);
    }
    process.env.PATH = `${LLVM_BIN_DIR}${path.delimiter}${process.env.PATH}`;
}

let toolchainReady = false;

function ensureToolchain() {
    if (toolchainReady) return;
    ensureCmake();
    ensureClangAndLld();
    if (!llvmToolchainConsistent()) {
        installLlvm(REQUIRED_LLVM_MAJOR);
        linkVersionedTools(REQUIRED_LLVM_MAJOR);
    }
    toolchainReady = true;
}

module.exports = { ensureToolchain };
