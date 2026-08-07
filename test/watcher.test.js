const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const fsp = require('node:fs/promises');
const os = require('node:os');
const path = require('node:path');
const { watchPaths } = require('../lib/watcher');

async function withTempDirs(count, fn) {
    const dirs = await Promise.all(
        Array.from({ length: count }, () => fsp.mkdtemp(path.join(os.tmpdir(), 'watch-test-')))
    );
    try {
        await fn(...dirs);
    } finally {
        await Promise.all(dirs.map((dir) => fsp.rm(dir, { recursive: true, force: true })));
    }
}

function waitForCall(changes, timeoutMs = 2000) {
    return new Promise((resolve, reject) => {
        const start = Date.now();
        const interval = setInterval(() => {
            if (changes.length > 0) {
                clearInterval(interval);
                resolve();
            } else if (Date.now() - start > timeoutMs) {
                clearInterval(interval);
                reject(new Error('timed out waiting for watcher callback'));
            }
        }, 20);
    });
}

test('watchPaths invokes the callback for a matching extension', async () => {
    await withTempDirs(1, async (dir) => {
        const changes = [];
        const watcher = watchPaths([dir], ['.c', '.h'], (filename) => changes.push(filename));
        try {
            fs.writeFileSync(path.join(dir, 'foo.c'), 'int x;');
            await waitForCall(changes);
            assert.ok(changes[0].endsWith('foo.c'));
        } finally {
            watcher.close();
        }
    });
});

test('watchPaths ignores files with a non-matching extension', async () => {
    await withTempDirs(1, async (dir) => {
        const changes = [];
        const watcher = watchPaths([dir], ['.c', '.h'], (filename) => changes.push(filename));
        try {
            fs.writeFileSync(path.join(dir, 'notes.txt'), 'hello');
            await assert.rejects(waitForCall(changes, 300));
            assert.deepEqual(changes, []);
        } finally {
            watcher.close();
        }
    });
});

test('watchPaths watches multiple roots and nested subdirectories', async () => {
    await withTempDirs(2, async (dirA, dirB) => {
        const changes = [];
        const watcher = watchPaths([dirA, dirB], ['.js'], (filename) => changes.push(filename));
        try {
            fs.mkdirSync(path.join(dirB, 'nested'));
            fs.writeFileSync(path.join(dirB, 'nested', 'bar.js'), 'console.log(1);');
            await waitForCall(changes);
            assert.ok(changes[0].endsWith(path.join('nested', 'bar.js')));
        } finally {
            watcher.close();
        }
    });
});

test('close() stops the callback from firing on further changes', async () => {
    await withTempDirs(1, async (dir) => {
        const changes = [];
        const watcher = watchPaths([dir], ['.c'], (filename) => changes.push(filename));
        watcher.close();
        fs.writeFileSync(path.join(dir, 'after-close.c'), 'int y;');
        await assert.rejects(waitForCall(changes, 300));
        assert.deepEqual(changes, []);
    });
});
