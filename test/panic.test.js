const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const WASM_PATH = path.join(__dirname, '..', 'build', 'app.wasm');
const MAIN_C_PATH = path.join(__dirname, '..', 'src', 'main.c');

function lineOf(needle) {
    const lines = fs.readFileSync(MAIN_C_PATH, 'utf8').split('\n');
    const index = lines.findIndex((line) => line.includes(needle));
    assert.ok(index !== -1, `expected to find ${JSON.stringify(needle)} in main.c`);
    return index + 1;
}

async function instantiate(reportPanic) {
    const bytes = fs.readFileSync(WASM_PATH);
    const { instance } = await WebAssembly.instantiate(bytes, {
        env: { report_panic: reportPanic },
    });
    return instance;
}

function decode(instance, ptr, len) {
    return new TextDecoder().decode(new Uint8Array(instance.exports.memory.buffer, ptr, len));
}

test('ASSERT failure reports the exact file, line, and stringified condition', async () => {
    const calls = [];
    let instance;
    const reportPanic = (filePtr, fileLen, line, msgPtr, msgLen) => {
        calls.push({
            file: decode(instance, filePtr, fileLen),
            line,
            msg: decode(instance, msgPtr, msgLen),
        });
        throw new Error('halt');
    };
    instance = await instantiate(reportPanic);

    assert.throws(() => instance.exports.debug_trigger_assert(), /halt/);
    assert.equal(calls.length, 1);
    assert.ok(calls[0].file.endsWith('src/main.c'));
    assert.equal(calls[0].line, lineOf('ASSERT(1 == 2)'));
    assert.equal(calls[0].msg, 'assertion failed: 1 == 2');
});

test('PANIC reports the exact file, line, and message', async () => {
    const calls = [];
    let instance;
    const reportPanic = (filePtr, fileLen, line, msgPtr, msgLen) => {
        calls.push({
            file: decode(instance, filePtr, fileLen),
            line,
            msg: decode(instance, msgPtr, msgLen),
        });
        throw new Error('halt');
    };
    instance = await instantiate(reportPanic);

    assert.throws(() => instance.exports.debug_trigger_panic(), /halt/);
    assert.equal(calls.length, 1);
    assert.ok(calls[0].file.endsWith('src/main.c'));
    assert.equal(calls[0].line, lineOf('PANIC("deliberate test panic")'));
    assert.equal(calls[0].msg, 'deliberate test panic');
});

test('wasm still halts via trap even if the JS import returns normally', async () => {
    const instance = await instantiate(() => {
        // deliberately does not throw, to exercise the __builtin_trap() fallback
    });

    assert.throws(() => instance.exports.debug_trigger_panic(), /unreachable/i);
});

test('a passing ASSERT does not report a panic', async () => {
    let panicked = false;
    const instance = await instantiate(() => {
        panicked = true;
    });

    instance.exports.init();
    assert.equal(panicked, false);
});
