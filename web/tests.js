async function run_tests() {

    function addTestRow(panel, name) {
        const row = document.createElement('div');
        row.style.cssText = 'padding: 4px 0;';

        const status = document.createElement('span');
        status.textContent = 'RUNNING';
        status.style.cssText = 'display: inline-block; width: 80px; color: #999;';

        row.appendChild(status);
        console.log(name);
        row.appendChild(document.createTextNode(name));
        panel.appendChild(row);
        return { row, status };
    }

    function markTestFailed(row, status, detail) {
        status.textContent = 'FAIL';
        status.style.color = '#ff6b6b';

        console.error(detail);

        const pre = document.createElement('pre');
        pre.textContent = detail;
        pre.style.cssText = 'margin: 0 0 8px 84px; color: #ff6b6b; white-space: pre-wrap;';
        row.after(pre);
    }

    async function resolveFrames(frames) {
        const response = await fetch('/__symbolicate', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ frames }),
        });
        const data = await response.json();
        if (!response.ok || data.error) {
            throw new Error(data.error || `symbolicate returned ${response.status}`);
        }
        return data.frames;
    }

    const panel = document.getElementById('test-panel');
    panel.style.cssText = 'font-family: ui-monospace, SFMono-Regular, Consolas, monospace; font-size: 14px; padding: 24px;';

    const wasmBytes = await fetch('/build/app.wasm').then((r) => r.arrayBuffer());

    await runWasmTests({
        wasmBytes,
        resolveFrames,
        debugLog: (message) => console.log(message),
        onResult({ name, passed, detail }) {
            if (!passed) {
                const { row, status } = addTestRow(panel, name);
                markTestFailed(row, status, detail);
            }
        },
        onComplete({ passed, failed, count }) {
            const summary = document.createElement('div');
            summary.style.cssText = 'font-weight: bold; margin-bottom: 16px;';
            summary.innerHTML =
                `<div style="color: #4caf50;">Tests Passed (${passed}/${count})</div>` +
                `<div style="color: #ff6b6b;">Tests Failed (${failed}/${count})</div>`;
            panel.prepend(summary);
        },
    });
}

run_tests();
