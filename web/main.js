let wasmInstance = null;

function decodeWasmString(ptr, len) {
    const bytes = new Uint8Array(wasmInstance.exports.memory.buffer, ptr, len);
    return new TextDecoder().decode(bytes);
}

function renderOverlay(text) {
    if (document.getElementById('panic-overlay')) {
        return;
    }

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

function parseTrapFrames(stack) {
    const re = /wasm-function\[(\d+)\]:0x([0-9a-fA-F]+)/g;
    const frames = [];
    let match;
    while ((match = re.exec(stack)) !== null) {
        frames.push({ funcIndex: Number(match[1]), offset: parseInt(match[2], 16) });
    }
    return frames;
}

function formatResolvedFrame(frame) {
    if (!frame.locations || frame.locations.length === 0) {
        return `  wasm-function[${frame.funcIndex}]:0x${frame.offset.toString(16)} (unresolved)`;
    }
    return frame.locations
        .map((loc) => `  ${loc.function} at ${loc.file}:${loc.line}:${loc.column}`)
        .join('\n');
}

async function handleWasmTrap(error) {
    const message = error.message || String(error);
    const rawFrames = parseTrapFrames(error.stack || '');

    if (rawFrames.length === 0) {
        renderOverlay(`TRAP\n\n${message}\n\n${error.stack || ''}`);
        return;
    }

    try {
        const response = await fetch('/__symbolicate', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ frames: rawFrames }),
        });
        const data = await response.json();
        if (!response.ok || data.error) {
            throw new Error(data.error || `symbolicate returned ${response.status}`);
        }
        const stackText = data.frames.map(formatResolvedFrame).join('\n');
        renderOverlay(`TRAP\n\n${message}\n\n${stackText}`);
    } catch (symbolicateErr) {
        renderOverlay(`TRAP\n\n${message}\n\n(symbolication failed: ${symbolicateErr.message})\n\n${error.stack || ''}`);
    }
}

function handleError(error) {
    if (error instanceof WebAssembly.RuntimeError) {
        handleWasmTrap(error);
        return;
    }
    console.error(error);
    if (document.getElementById('panic-overlay')) {
        return;
    }
    renderOverlay(`ERROR\n\n${error.message || String(error)}\n\n${error.stack || ''}`);
}

window.addEventListener('error', (event) => {
    event.preventDefault();
    handleError(event.error ?? new Error(event.message));
});

window.addEventListener('unhandledrejection', (event) => {
    event.preventDefault();
    handleError(event.reason instanceof Error ? event.reason : new Error(String(event.reason)));
});

const importObject = {
    env: {
        report_panic(filePtr, fileLen, line, msgPtr, msgLen) {
            const file = decodeWasmString(filePtr, fileLen);
            const message = decodeWasmString(msgPtr, msgLen);
            renderOverlay(`PANIC\n\n${file}:${line}\n\n${message}`);
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
    });
