const fs = require('node:fs');
const path = require('node:path');
const { spawnSync } = require('node:child_process');

const args = process.argv.slice(2);
const mode = args.includes('--release') ? 'Release' : 'Debug';
const verbose = args.includes('--verbose');
const unity = args.includes('--unity');
const tests = args.includes('--tests');
const fuzzTests = args.includes('--fuzz-tests');
const coverage = args.includes('--coverage');
const assertions = args.includes('--assertions');
const debugSymbols = args.includes('--debug-symbols');

const BUILD_DIR = 'build';
const FLAGS_MARKER = path.join(BUILD_DIR, '.build-flags');
// Encodes every flag that changes compiled output (not --verbose). CMake's
// incremental rebuild doesn't reliably invalidate every object when e.g.
// APP_COVERAGE flips between runs against the same build dir (a stale,
// differently-instrumented .o can survive and fail to link) -- so instead
// of trusting it across configurations, wipe build/ whenever this
// signature differs from the last build that ran there.
const flagsSignature = JSON.stringify({ mode, unity, tests, fuzzTests, coverage, assertions, debugSymbols });

function run(cmd, cmdArgs) {
    const result = spawnSync(cmd, cmdArgs, { encoding: 'utf8', stdio: verbose ? 'inherit' : 'pipe' });
    if (result.status !== 0) {
        if (!verbose) {
            process.stdout.write(result.stdout);
            process.stderr.write(result.stderr);
        }
        process.exit(result.status ?? 1);
    }
}

const previousSignature = fs.existsSync(FLAGS_MARKER) ? fs.readFileSync(FLAGS_MARKER, 'utf8') : null;
if (previousSignature !== flagsSignature && fs.existsSync(BUILD_DIR)) {
    fs.rmSync(BUILD_DIR, { recursive: true, force: true });
}

run('cmake', ['-S', '.', '-B', BUILD_DIR, `-DCMAKE_BUILD_TYPE=${mode}`, `-DAPP_UNITY_BUILD=${unity ? 'ON' : 'OFF'}`, `-DAPP_BUILD_TESTS=${tests ? 'ON' : 'OFF'}`, `-DAPP_BUILD_FUZZ_TESTS=${fuzzTests ? 'ON' : 'OFF'}`, `-DAPP_COVERAGE=${coverage ? 'ON' : 'OFF'}`, `-DAPP_ASSERTIONS=${assertions ? 'ON' : 'OFF'}`, `-DAPP_DEBUG_SYMBOLS=${debugSymbols ? 'ON' : 'OFF'}`]);
run('cmake', ['--build', BUILD_DIR]);

fs.writeFileSync(FLAGS_MARKER, flagsSignature);
