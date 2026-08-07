const test = require('node:test');
const assert = require('node:assert/strict');
const { debounce } = require('../lib/debounce');

test('debounce delays the call until the wait period elapses', (t) => {
    t.mock.timers.enable({ apis: ['setTimeout'] });
    let calls = 0;
    const debounced = debounce(() => { calls += 1; }, 100);

    debounced();
    assert.equal(calls, 0);

    t.mock.timers.tick(99);
    assert.equal(calls, 0);

    t.mock.timers.tick(1);
    assert.equal(calls, 1);
});

test('debounce coalesces rapid calls into a single invocation', (t) => {
    t.mock.timers.enable({ apis: ['setTimeout'] });
    let calls = 0;
    const debounced = debounce(() => { calls += 1; }, 100);

    debounced();
    t.mock.timers.tick(50);
    debounced();
    t.mock.timers.tick(50);
    assert.equal(calls, 0, 'timer should have been reset by the second call');

    t.mock.timers.tick(50);
    assert.equal(calls, 1);
});

test('debounce forwards the latest call arguments', (t) => {
    t.mock.timers.enable({ apis: ['setTimeout'] });
    const seen = [];
    const debounced = debounce((value) => seen.push(value), 100);

    debounced('first');
    debounced('second');
    t.mock.timers.tick(100);

    assert.deepEqual(seen, ['second']);
});

test('debounce.cancel prevents a pending call from firing', (t) => {
    t.mock.timers.enable({ apis: ['setTimeout'] });
    let calls = 0;
    const debounced = debounce(() => { calls += 1; }, 100);

    debounced();
    debounced.cancel();
    t.mock.timers.tick(100);

    assert.equal(calls, 0);
});
