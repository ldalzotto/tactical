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
        console.error('[setup-toolchain] python3 cannot import apt_pkg and no matching interpreter was found; add-apt-repository may fail.');
        return;
    }
    const interpreter = `/usr/bin/python${match[1]}.${match[2]}`;
    if (!fs.existsSync(interpreter)) {
        console.error(`[setup-toolchain] apt_pkg needs ${interpreter}, which is not installed; add-apt-repository may fail.`);
        return;
    }
    console.log(`[setup-toolchain] python3 alternative can't import apt_pkg; repointing it at ${interpreter}...`);
    run('sudo', ['update-alternatives', '--install', '/usr/bin/python3', 'python3', interpreter, '1']);
    run('sudo', ['update-alternatives', '--set', 'python3', interpreter]);
    if (run('/usr/bin/python3', ['-c', 'import apt_pkg']).status !== 0) {
        console.error('[setup-toolchain] Repointing python3 did not fix apt_pkg; add-apt-repository may fail.');
    }
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

// A previous run of this script may have already pinned LLVM 22 into
// LLVM_BIN_DIR (see linkVersionedTools). Trust those symlinks without
// re-shelling out to apt on every build.js/coverage.js invocation --
// update-alternatives on most distros doesn't repoint the unversioned
// `clang`/`llvm-cov`/`llvm-profdata` names to a newly apt-installed
// versioned package, so checking PATH alone would otherwise reinstall
// LLVM 22 (a slow, network-dependent no-op) on every single run.
function pinnedToolchainReady() {
    return ['clang', 'llvm-cov', 'llvm-profdata'].every((name) => {
        const link = path.join(LLVM_BIN_DIR, name);
        return fs.existsSync(link) && majorVersion(link) === REQUIRED_LLVM_MAJOR;
    });
}

// build.js compiles with `clang` and coverage.js reads the resulting
// profile with `llvm-cov`/`llvm-profdata`; a version mismatch between them
// makes coverage.js report "no coverage data found". Only reach for a
// specific LLVM install when the toolchain already on PATH isn't
// self-consistent. Callers check pinnedToolchainReady() first.
function llvmToolchainConsistent() {
    if (!commandExists('clang') || !commandExists('llvm-cov') || !commandExists('llvm-profdata')) return false;
    const clangVer = majorVersion('clang');
    const covVer = majorVersion('llvm-cov');
    const profVer = majorVersion('llvm-profdata');
    return clangVer === REQUIRED_LLVM_MAJOR && clangVer === covVer && clangVer === profVer;
}

function installLlvm(major) {
    console.log(`[setup-toolchain] Installing LLVM ${major}...`);
    ensureAddAptRepositoryPython();
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
// their unversioned names in a dedicated directory so cmake/build.js/
// coverage.js can resolve the pinned version instead of whatever else
// update-alternatives has registered.
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
}

function prependPinnedToolchainToPath() {
    process.env.PATH = `${LLVM_BIN_DIR}${path.delimiter}${process.env.PATH}`;
}

let toolchainReady = false;

function ensureToolchain() {
    if (toolchainReady) return;
    ensureCmake();
    ensureClangAndLld();
    const pinnedReady = pinnedToolchainReady();
    if (!pinnedReady && !llvmToolchainConsistent()) {
        installLlvm(REQUIRED_LLVM_MAJOR);
        linkVersionedTools(REQUIRED_LLVM_MAJOR);
        prependPinnedToolchainToPath();
    } else if (pinnedReady) {
        prependPinnedToolchainToPath();
    }
    toolchainReady = true;
}

module.exports = { ensureToolchain };
