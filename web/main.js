WebAssembly.instantiateStreaming(fetch('/build/app.wasm'), {})
    .then(({ instance }) => {
        console.log('smoke_test() =', instance.exports.smoke_test());
    })
    .catch((err) => {
        console.error('failed to load app.wasm', err);
    });
