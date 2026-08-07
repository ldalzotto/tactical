function debounce(fn, waitMs) {
    let timer = null;

    function debounced(...args) {
        if (timer) clearTimeout(timer);
        timer = setTimeout(() => {
            timer = null;
            fn(...args);
        }, waitMs);
    }

    debounced.cancel = () => {
        if (timer) clearTimeout(timer);
        timer = null;
    };

    return debounced;
}

module.exports = { debounce };
