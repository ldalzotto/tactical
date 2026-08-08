function addTestRow(panel, name) {
    const row = document.createElement('div');
    row.style.cssText = 'padding: 4px 0;';

    const status = document.createElement('span');
    status.textContent = 'RUNNING';
    status.style.cssText = 'display: inline-block; width: 80px; color: #999;';

    row.appendChild(status);
    row.appendChild(document.createTextNode(name));
    panel.appendChild(row);
    return { row, status };
}

function markTestFailed(row, status, message) {
    status.textContent = 'FAIL';
    status.style.color = '#ff6b6b';

    const detail = document.createElement('pre');
    detail.textContent = message;
    detail.style.cssText = 'margin: 0 0 8px 84px; color: #ff6b6b; white-space: pre-wrap;';
    row.after(detail);
}

function markTestPassed(status) {
    status.textContent = 'PASS';
    status.style.color = '#4caf50';
}

function runTests(instance) {
    const panel = document.getElementById('test-panel');
    panel.style.cssText = 'font-family: ui-monospace, SFMono-Regular, Consolas, monospace; font-size: 14px; padding: 24px;';

    const { test_discovery_count, test_discovery_name_begin, test_discovery_name_end, test_discovery_fn_at, test_run } = instance.exports;
    const count = test_discovery_count();

    for (let i = 0; i < count; i++) {
        const beginPtr = test_discovery_name_begin(i);
        const endPtr = test_discovery_name_end(i);
        const name = decodeWasmString(beginPtr, endPtr - beginPtr);
        const { row, status } = addTestRow(panel, name);

        const fn = test_discovery_fn_at(i);
        try {
            test_run(fn);
            markTestPassed(status);
        } catch (err) {
            markTestFailed(row, status, err.message || String(err));
        }
    }
}

WebAssembly.instantiateStreaming(fetch('/build/app.wasm'), importObject)
    .then(({ instance }) => runTests(instance));
