'use strict';

// Ensures the native toolchain (cmake, clang, lld, matching
// llvm-cov/llvm-profdata, and clang-include-cleaner) is available before
// build.js/coverage.js/iwyu.js shell out to it. Installs missing pieces via
// apt when possible (CI runners), and is a fast no-op when a working,
// version-consistent toolchain is already on PATH (e.g. this sandbox, or a
// dev machine).

const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');

const REQUIRED_LLVM_MAJOR = '22';
const LLVM_BIN_DIR = path.join(os.homedir(), '.cache', 'app-llvm22-bin');

let verbose = false;

function log(...args) {
    if (verbose) console.log(...args);
}

function logError(...args) {
    if (verbose) console.error(...args);
}

function run(cmd, args, opts = {}) {
    return spawnSync(cmd, args, { encoding: 'utf8', ...opts });
}

function runLogged(cmd, args, opts = {}) {
    return run(cmd, args, { stdio: verbose ? 'inherit' : 'ignore', ...opts });
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
    return runLogged('sudo', ['apt-get', ...args]);
}

// llvm.sh (see installLlvm) shells out to `add-apt-repository`, whose
// shebang is `/usr/bin/python3`. Some sandbox images register a
// `python3` alternative (e.g. 3.11) that doesn't match the Python version
// python3-apt's compiled `apt_pkg` extension was built against (e.g.
// 3.12), so `import apt_pkg` fails with ModuleNotFoundError and
// add-apt-repository dies before the LLVM repo is ever added. Detect that
// and repoint the `python3` alternative at whichever interpreter can
// actually import apt_pkg.
function ensureAddAptRepositoryPython() {
    if (run('/usr/bin/python3', ['-c', 'import apt_pkg']).status === 0) return;
    const found = run('bash', [
        '-c',
        "find /usr/lib/python3*/dist-packages /usr/lib/python3/dist-packages -maxdepth 1 -name 'apt_pkg.cpython-*.so' 2>/dev/null | head -n1",
    ]);
    const soPath = (found.stdout || '').trim();
    const match = soPath.match(/cpython-(\d)(\d+)/);
    if (!match) {
        logError('[setup-toolchain] python3 cannot import apt_pkg and no matching interpreter was found; add-apt-repository may fail.');
        return;
    }
    const interpreter = `/usr/bin/python${match[1]}.${match[2]}`;
    if (!fs.existsSync(interpreter)) {
        logError(`[setup-toolchain] apt_pkg needs ${interpreter}, which is not installed; add-apt-repository may fail.`);
        return;
    }
    log(`[setup-toolchain] python3 alternative can't import apt_pkg; repointing it at ${interpreter}...`);
    run('sudo', ['update-alternatives', '--install', '/usr/bin/python3', 'python3', interpreter, '1']);
    run('sudo', ['update-alternatives', '--set', 'python3', interpreter]);
    if (run('/usr/bin/python3', ['-c', 'import apt_pkg']).status !== 0) {
        logError('[setup-toolchain] Repointing python3 did not fix apt_pkg; add-apt-repository may fail.');
    }
}

function ensureCmake() {
    if (commandExists('cmake')) return;
    log('[setup-toolchain] cmake not found, installing...');
    apt(['update']);
    if (apt(['install', '-y', 'cmake']).status !== 0) {
        console.error('[setup-toolchain] Failed to install cmake. Install it manually and retry.');
        process.exit(1);
    }
}

function ensureClangAndLld() {
    if (commandExists('clang') && commandExists('lld')) return;
    log('[setup-toolchain] clang/lld not found, installing...');
    apt(['update']);
    if (apt(['install', '-y', 'clang', 'lld']).status !== 0) {
        console.error('[setup-toolchain] Failed to install clang/lld. Install them manually and retry.');
        process.exit(1);
    }
}

// wasm-objdump (used by symbolicate.js to read wasm function names/offsets
// for calibrating DWARF address resolution) ships in WABT, a separate apt
// package from the LLVM toolchain.
function ensureWabt() {
    if (commandExists('wasm-objdump')) return;
    log('[setup-toolchain] wasm-objdump not found, installing wabt...');
    apt(['update']);
    if (apt(['install', '-y', 'wabt']).status !== 0) {
        console.error('[setup-toolchain] Failed to install wabt. Install it manually and retry.');
        process.exit(1);
    }
}

// build.js compiles with `clang`, coverage.js reads the resulting profile
// with `llvm-cov`/`llvm-profdata`, iwyu.js parses with
// `clang-include-cleaner` (must match `clang`'s AST), and symbolicate.js
// resolves wasm stack traces with `llvm-symbolizer` (must match the DWARF
// `clang` emitted -- a version mismatch there has been observed to
// misattribute resolved function names, not just fail outright). A version
// mismatch among any of these makes coverage.js report "no coverage data
// found", makes iwyu.js misbehave, or corrupts symbolicated traces, so all
// five are always kept on the same LLVM major.
const ALL_TOOLS = ['clang', 'llvm-cov', 'llvm-profdata', 'clang-include-cleaner', 'llvm-symbolizer'];

// A previous run of this script may have already pinned LLVM 22 into
// LLVM_BIN_DIR (see linkVersionedTools). Trust those symlinks without
// re-shelling out to apt on every build.js/coverage.js/iwyu.js invocation --
// update-alternatives on most distros doesn't repoint the unversioned
// tool names to a newly apt-installed versioned package, so checking PATH
// alone would otherwise reinstall LLVM 22 (a slow, network-dependent
// no-op) on every single run.
function pinnedToolchainReady() {
    return ALL_TOOLS.every((name) => {
        const link = path.join(LLVM_BIN_DIR, name);
        return fs.existsSync(link) && majorVersion(link) === REQUIRED_LLVM_MAJOR;
    });
}

// Only reach for a specific LLVM install when the toolchain already on
// PATH isn't self-consistent. Callers check pinnedToolchainReady() first.
function llvmToolchainConsistent() {
    return ALL_TOOLS.every((name) => commandExists(name) && majorVersion(name) === REQUIRED_LLVM_MAJOR);
}

function installLlvm(major) {
    log(`[setup-toolchain] Installing LLVM ${major}...`);
    ensureAddAptRepositoryPython();
    const scriptPath = path.join(os.tmpdir(), 'llvm.sh');
    if (runLogged('wget', ['-O', scriptPath, 'https://apt.llvm.org/llvm.sh']).status !== 0) {
        console.error('[setup-toolchain] Failed to download the LLVM install script.');
        process.exit(1);
    }
    fs.chmodSync(scriptPath, 0o755);
    const install = runLogged('sudo', [scriptPath, major, 'all']);
    fs.rmSync(scriptPath, { force: true });
    if (install.status !== 0) {
        console.error(`[setup-toolchain] Failed to install LLVM ${major}. Install it manually and retry.`);
        process.exit(1);
    }
}

// Symlinks the versioned LLVM tool names (clang-22, llvm-cov-22, ...) to
// their unversioned names in a dedicated directory so cmake/build.js/
// coverage.js/iwyu.js can resolve the pinned version instead of whatever
// else update-alternatives has registered.
function linkVersionedTools(major) {
    fs.mkdirSync(LLVM_BIN_DIR, { recursive: true });
    for (const name of ALL_TOOLS) {
        const versioned = `/usr/bin/${name}-${major}`;
        if (!fs.existsSync(versioned)) {
            console.error(`[setup-toolchain] Expected ${versioned} after installing LLVM ${major}.`);
            process.exit(1);
        }
        const dest = path.join(LLVM_BIN_DIR, name);
        fs.rmSync(dest, { force: true });
        fs.symlinkSync(versioned, dest);
    }
}

function prependPinnedToolchainToPath() {
    process.env.PATH = `${LLVM_BIN_DIR}${path.delimiter}${process.env.PATH}`;
}

// clang-include-cleaner ships in the clang-tools package rather than the
// core LLVM one, so it isn't guaranteed by installLlvm()'s `all` component
// install. Install it explicitly so linkVersionedTools() can find it
// alongside clang/llvm-cov/llvm-profdata.
function ensureClangTools(major) {
    if (fs.existsSync(`/usr/bin/clang-include-cleaner-${major}`)) return;
    log(`[setup-toolchain] clang-include-cleaner-${major} not found, installing clang-tools-${major}...`);
    ensureAddAptRepositoryPython();
    apt(['update']);
    if (apt(['install', '-y', `clang-tools-${major}`]).status !== 0) {
        console.error(`[setup-toolchain] Failed to install clang-tools-${major} (for clang-include-cleaner). Install it manually and retry.`);
        process.exit(1);
    }
}

let toolchainReady = false;

function ensureToolchain({ verbose: verboseOpt = false } = {}) {
    if (toolchainReady) return;
    verbose = verboseOpt;
    ensureCmake();
    ensureClangAndLld();
    ensureWabt();
    if (pinnedToolchainReady()) {
        prependPinnedToolchainToPath();
    } else if (!llvmToolchainConsistent()) {
        installLlvm(REQUIRED_LLVM_MAJOR);
        ensureClangTools(REQUIRED_LLVM_MAJOR);
        linkVersionedTools(REQUIRED_LLVM_MAJOR);
        prependPinnedToolchainToPath();
    }
    toolchainReady = true;
}

module.exports = { ensureToolchain };
