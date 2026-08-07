const fs = require('node:fs');
const path = require('node:path');

function watchPaths(roots, extensions, onChange) {
    const watchers = roots.map((root) =>
        fs.watch(root, { recursive: true }, (eventType, filename) => {
            if (!filename) return;
            if (extensions.includes(path.extname(filename))) {
                onChange(filename);
            }
        })
    );

    return {
        close() {
            for (const watcher of watchers) watcher.close();
        },
    };
}

module.exports = { watchPaths };
