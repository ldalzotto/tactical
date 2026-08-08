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

async function markTestFailed(row, status, err) {
    status.textContent = 'FAIL';
    status.style.color = '#ff6b6b';

    let text = err.message || String(err);

    const rawFrames = parseTrapFrames(err.stack || '');
    if (rawFrames.length > 0) {
        try {
            text += '\n\n' + await symbolicateFrames(rawFrames);
        } catch (symbolicateErr) {
            text += `\n\n(symbolication failed: ${symbolicateErr.message})`;
        }
    }
    console.error(text);

    const detail = document.createElement('pre');
    detail.textContent = text;
    detail.style.cssText = 'margin: 0 0 8px 84px; color: #ff6b6b; white-space: pre-wrap;';
    row.after(detail);
}

async function runTests(instance) {
    const panel = document.getElementById('test-panel');
    panel.style.cssText = 'font-family: ui-monospace, SFMono-Regular, Consolas, monospace; font-size: 14px; padding: 24px;';

    const { test_discovery_count, test_discovery_name_begin, test_discovery_name_end, test_discovery_fn_at, test_run } = instance.exports;
    const count = test_discovery_count();

    let passed = 0;
    let failed = 0;

    for (let i = 0; i < count; i++) {
        const beginPtr = test_discovery_name_begin(i);
        const endPtr = test_discovery_name_end(i);
        const name = decodeWasmString(beginPtr, endPtr - beginPtr);

        const fn = test_discovery_fn_at(i);
        try {
            test_run(fn);
            passed++;
        } catch (err) {
            failed++;
            const { row, status } = addTestRow(panel, name);
            await markTestFailed(row, status, err);
        }
    }

    const summary = document.createElement('div');
    summary.style.cssText = 'font-weight: bold; margin-bottom: 16px;';
    summary.innerHTML =
        `<div style="color: #4caf50;">Tests Passed (${passed}/${count})</div>` +
        `<div style="color: #ff6b6b;">Tests Failed (${failed}/${count})</div>`;
    panel.prepend(summary);
}

WebAssembly.instantiateStreaming(fetch('/build/app.wasm'), importObject)
    .then(({ instance }) => runTests(instance));
