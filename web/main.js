WebAssembly.instantiateStreaming(fetch('/build/app.wasm'), {})
    .then(({ instance }) => {
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
