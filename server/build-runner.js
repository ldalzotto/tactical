const { spawn } = require('node:child_process');
const path = require('node:path');

function runBuild({ cwd, stdio = 'inherit', release = false, verbose = false } = {}) {
    return new Promise((resolve, reject) => {
        const args = [path.join(__dirname, 'build.js')];
        if (release) args.push('--release');
        if (verbose) args.push('--verbose');
        const proc = spawn('node', args, { cwd, stdio });

        proc.on('error', reject);
        proc.on('exit', (code) => {
            if (code === 0) {
                resolve();
            } else {
                reject(new Error(`build exited with exit code ${code}`));
            }
        });
    });
}

module.exports = { runBuild };
