let wasmInstance = null;

WebAssembly.instantiateStreaming(fetch('/build/app.wasm'), importObject)
    .then(({ instance }) => {
        wasmInstance = instance;
        const { init, onNextFrame } = instance.exports;

        const statePtr = init(memory.buffer.byteLength, Math.floor(performance.now()));

        function tick() {
            const nowMs = Math.floor(performance.now());
            const waitMs = onNextFrame(statePtr, nowMs);
            setTimeout(tick, waitMs);
        }

        tick();
    });
