const { spawnSync } = require('node:child_process');

const args = process.argv.slice(2);
const mode = args.includes('--release') ? 'Release' : 'Debug';
const verbose = args.includes('--verbose');
const unity = args.includes('--unity');
const tests = args.includes('--tests');
const coverage = args.includes('--coverage');

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

run('cmake', ['-S', '.', '-B', 'build', `-DCMAKE_BUILD_TYPE=${mode}`, `-DAPP_UNITY_BUILD=${unity ? 'ON' : 'OFF'}`, `-DAPP_BUILD_TESTS=${tests ? 'ON' : 'OFF'}`, `-DAPP_COVERAGE=${coverage ? 'ON' : 'OFF'}`]);
run('cmake', ['--build', 'build']);
