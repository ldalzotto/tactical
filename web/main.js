let wasmInstance = null;

function decodeWasmString(ptr, len) {
    const bytes = new Uint8Array(wasmInstance.exports.memory.buffer, ptr, len);
    return new TextDecoder().decode(bytes);
}

function showPanicOverlay(file, line, message) {
    if (document.getElementById('panic-overlay')) {
        return;
    }

    const text = `PANIC\n\n${file}:${line}\n\n${message}`;

    const overlay = document.createElement('div');
    overlay.id = 'panic-overlay';
    overlay.style.cssText = [
        'position: fixed',
        'inset: 0',
        'background: rgba(20, 0, 0, 0.94)',
        'color: #ff6b6b',
        'font-family: ui-monospace, SFMono-Regular, Consolas, monospace',
        'font-size: 14px',
        'line-height: 1.5',
        'padding: 24px',
        'box-sizing: border-box',
        'z-index: 2147483647',
        'overflow: auto',
        'user-select: text',
    ].join(';');

    const pre = document.createElement('pre');
    pre.textContent = text;
    pre.style.cssText = 'margin: 0 0 16px 0; white-space: pre-wrap; word-break: break-word;';

    const copyButton = document.createElement('button');
    copyButton.textContent = 'Copy';
    copyButton.style.cssText = 'font-family: inherit; font-size: 13px; padding: 6px 14px; cursor: pointer;';
    copyButton.addEventListener('click', () => {
        navigator.clipboard.writeText(text)
            .then(() => {
                copyButton.textContent = 'Copied!';
            })
            .catch(() => {
                copyButton.textContent = 'Copy failed';
            });
    });

    overlay.appendChild(pre);
    overlay.appendChild(copyButton);
    document.body.appendChild(overlay);
}

const importObject = {
    env: {
        report_panic(filePtr, fileLen, line, msgPtr, msgLen) {
            const file = decodeWasmString(filePtr, fileLen);
            const message = decodeWasmString(msgPtr, msgLen);
            showPanicOverlay(file, line, message);
            throw new Error(`panic: ${file}:${line}: ${message}`);
        },
    },
};

WebAssembly.instantiateStreaming(fetch('/build/app.wasm'), importObject)
    .then(({ instance }) => {
        wasmInstance = instance;
        const { memory, get_framebuffer, get_width, get_height, init, frame } = instance.exports;

        const width = get_width();
        const height = get_height();

        const canvas = document.getElementById('screen');
        canvas.width = width;
        canvas.height = height;
        const ctx = canvas.getContext('2d');
        const imageData = ctx.createImageData(width, height);

        const fbPtr = get_framebuffer();
        const fbBytes = width * height * 4;

        init();

        let lastTimestamp = null;

        function tick(timestamp) {
            const dtMs = lastTimestamp === null ? 0 : timestamp - lastTimestamp;
            lastTimestamp = timestamp;

            frame(dtMs);

            imageData.data.set(new Uint8ClampedArray(memory.buffer, fbPtr, fbBytes));
            ctx.putImageData(imageData, 0, 0);

            requestAnimationFrame(tick);
        }

        requestAnimationFrame(tick);
    })
    .catch((err) => {
        console.error('failed to load app.wasm', err);
    });
