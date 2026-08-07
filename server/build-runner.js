const { spawn: defaultSpawn } = require('node:child_process');

function runBuild({ spawn = defaultSpawn, cwd, stdio = 'inherit' } = {}) {
    return new Promise((resolve, reject) => {
        const proc = spawn('npm', ['run', 'build'], { cwd, stdio });

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
