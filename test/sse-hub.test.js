const test = require('node:test');
const assert = require('node:assert/strict');
const { EventEmitter } = require('node:events');
const { createSSEHub } = require('../lib/sse-hub');

class FakeRes extends EventEmitter {
    constructor() {
        super();
        this.statusCode = null;
        this.headers = null;
        this.chunks = [];
        this.ended = false;
    }

    writeHead(statusCode, headers) {
        this.statusCode = statusCode;
        this.headers = headers;
    }

    write(chunk) {
        this.chunks.push(chunk);
        return true;
    }

    end() {
        this.ended = true;
    }
}

class FakeReq extends EventEmitter {}

test('handle opens an SSE stream with the correct headers', () => {
    const hub = createSSEHub();
    const req = new FakeReq();
    const res = new FakeRes();

    hub.handle(req, res);

    assert.equal(res.statusCode, 200);
    assert.equal(res.headers['Content-Type'], 'text/event-stream');
    assert.equal(res.headers['Cache-Control'], 'no-cache');
    assert.equal(res.headers['Connection'], 'keep-alive');
});

test('broadcast writes the event to every connected client', () => {
    const hub = createSSEHub();
    const req1 = new FakeReq();
    const res1 = new FakeRes();
    const req2 = new FakeReq();
    const res2 = new FakeRes();

    hub.handle(req1, res1);
    hub.handle(req2, res2);
    hub.broadcast('reload');

    for (const res of [res1, res2]) {
        const payload = res.chunks.join('');
        assert.match(payload, /event: reload/);
        assert.match(payload, /data: /);
    }
});

test('a closed connection is removed and no longer receives broadcasts', () => {
    const hub = createSSEHub();
    const req = new FakeReq();
    const res = new FakeRes();

    hub.handle(req, res);
    assert.equal(hub.size, 1);

    req.emit('close');
    assert.equal(hub.size, 0);

    res.chunks.length = 0;
    hub.broadcast('reload');
    assert.deepEqual(res.chunks, []);
});
