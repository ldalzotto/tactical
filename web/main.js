// Wasm memory is imported (not module-owned), so JS decides the budget up
// front. 32 pages (2 MiB) comfortably covers the module's static data/stack
// plus the framebuffer with headroom; bump it if the app grows. If it's ever
// too small, the C-side linear allocator asserts/traps instead of silently
// corrupting memory, and the existing trap overlay below will show it.
const MEMORY_PAGES = 32;
const memory = new WebAssembly.Memory({ initial: MEMORY_PAGES });

let wasmInstance = null;

function decodeWasmString(ptr, len) {
    const bytes = new Uint8Array(memory.buffer, ptr, len);
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

let canvas = null;
let ctx = null;
let imageData = null;
let fbBytes = 0;

const importObject = {
    env: {
        memory,
        report_panic(filePtr, fileLen, line, msgPtr, msgLen) {
            const file = decodeWasmString(filePtr, fileLen);
            const message = decodeWasmString(msgPtr, msgLen);
            renderOverlay(`PANIC\n\n${file}:${line}\n\n${message}`);
            throw new Error(`panic: ${file}:${line}: ${message}`);
        },
        create_window(width, height) {
            canvas = document.getElementById('screen');
            canvas.width = width;
            canvas.height = height;
            ctx = canvas.getContext('2d');
            imageData = ctx.createImageData(width, height);
            fbBytes = width * height * 4;
        },
    },
};

WebAssembly.instantiateStreaming(fetch('/build/app.wasm'), importObject)
    .then(({ instance }) => {
        wasmInstance = instance;
        const { get_framebuffer, init, frame } = instance.exports;

        const statePtr = init(memory.buffer.byteLength);
        const fbPtr = get_framebuffer(statePtr);

        let lastTimestamp = null;

        function tick(timestamp) {
            const dtMs = lastTimestamp === null ? 0 : timestamp - lastTimestamp;
            lastTimestamp = timestamp;

            frame(statePtr, dtMs);

            imageData.data.set(new Uint8ClampedArray(memory.buffer, fbPtr, fbBytes));
            ctx.putImageData(imageData, 0, 0);

            requestAnimationFrame(tick);
        }

        requestAnimationFrame(tick);
    });
