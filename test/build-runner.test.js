const test = require('node:test');
const assert = require('node:assert/strict');
const { EventEmitter } = require('node:events');
const { runBuild } = require('../lib/build-runner');

function fakeSpawn(exitCode) {
    const calls = [];
    const spawn = (command, args, options) => {
        calls.push({ command, args, options });
        const proc = new EventEmitter();
        queueMicrotask(() => proc.emit('exit', exitCode));
        return proc;
    };
    spawn.calls = calls;
    return spawn;
}

test('runBuild resolves when the build process exits with code 0', async () => {
    const spawn = fakeSpawn(0);
    await assert.doesNotReject(runBuild({ spawn, cwd: '/repo' }));
    assert.equal(spawn.calls.length, 1);
    assert.equal(spawn.calls[0].command, 'npm');
    assert.deepEqual(spawn.calls[0].args, ['run', 'build']);
    assert.equal(spawn.calls[0].options.cwd, '/repo');
});

test('runBuild rejects when the build process exits with a non-zero code', async () => {
    const spawn = fakeSpawn(1);
    await assert.rejects(runBuild({ spawn, cwd: '/repo' }), /exit code 1/);
});

test('runBuild rejects when the process itself errors out', async () => {
    const spawn = () => {
        const proc = new EventEmitter();
        queueMicrotask(() => proc.emit('error', new Error('spawn failed')));
        return proc;
    };
    await assert.rejects(runBuild({ spawn, cwd: '/repo' }), /spawn failed/);
});
