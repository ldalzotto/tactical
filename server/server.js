const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');

const { createSSEHub } = require('./sse-hub');
const { watchPaths } = require('./watcher');
const { debounce } = require('./debounce');
const { runBuild } = require('./build-runner');
const { symbolicate } = require('./symbolicate');

const PORT = process.env.PORT || 8081;
const ROOT = path.join(__dirname, '..');

const RELOAD_EVENTS_PATH = '/__reload';
const SYMBOLICATE_PATH = '/__symbolicate';
const DEBUG_LOG_PATH = '/__debug_log';
const MAX_SYMBOLICATE_BODY_BYTES = 64 * 1024;
const WATCH_ROOTS = [path.join(ROOT, 'src'), path.join(ROOT, 'web')];
const WATCH_EXTENSIONS = ['.c', '.h', '.js', '.html'];
const REBUILD_DEBOUNCE_MS = 100;

const ROUTES = [
    { prefix: '/build/', dir: path.join(ROOT, 'build') },
    { prefix: '/', dir: path.join(ROOT, 'web') },
];

const CONTENT_TYPES = {
    '.html': 'text/html; charset=utf-8',
    '.js': 'text/javascript; charset=utf-8',
    '.wasm': 'application/wasm',
};

function resolveFile(urlPath) {
    const route = ROUTES.find((r) => urlPath.startsWith(r.prefix));
    const relative = urlPath.slice(route.prefix.length) || 'index.html';
    const filePath = path.join(route.dir, relative);

    if (!filePath.startsWith(route.dir + path.sep) && filePath !== route.dir) {
        return null;
    }
    return filePath;
}

function handleDebugLog(req, res) {
    let body = '';

    req.on('data', (chunk) => {
        body += chunk;
    });

    req.on('end', () => {
        try {
            console.log(body);
            res.writeHead(200);
            res.end();
        } catch (err) {
            res.writeHead(400);
            res.end();
        }
    });
}

function handleSymbolicate(req, res) {
    let body = '';
    let tooLarge = false;

    req.on('data', (chunk) => {
        body += chunk;
        if (body.length > MAX_SYMBOLICATE_BODY_BYTES) {
            tooLarge = true;
            res.writeHead(413);
            res.end('Payload too large');
            req.destroy();
        }
    });

    req.on('end', async () => {
        if (tooLarge) return;

        try {
            const { frames } = JSON.parse(body);
            const resolved = await symbolicate({ wasmPath: path.join(ROOT, 'build', 'app.wasm'), frames });
            res.writeHead(200, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify({ frames: resolved }));
        } catch (err) {
            res.writeHead(500, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify({ error: err.message }));
        }
    });
}

const sseHub = createSSEHub();

const server = http.createServer((req, res) => {
    const urlPath = decodeURIComponent(req.url.split('?')[0]);

    if (urlPath === RELOAD_EVENTS_PATH) {
        sseHub.handle(req, res);
        return;
    }

    if (urlPath === DEBUG_LOG_PATH && req.method === 'POST') {
        handleDebugLog(req, res);
        return;
    }

    if (urlPath === SYMBOLICATE_PATH && req.method === 'POST') {
        handleSymbolicate(req, res);
        return;
    }

    const filePath = resolveFile(urlPath);

    if (!filePath) {
        res.writeHead(403);
        res.end('Forbidden');
        return;
    }

    fs.readFile(filePath, (err, data) => {
        if (err) {
            res.writeHead(404);
            res.end('Not found');
            return;
        }
        const ext = path.extname(filePath);
        res.writeHead(200, { 'Content-Type': CONTENT_TYPES[ext] || 'application/octet-stream' });
        res.end(data);
    });
});

const rebuild = debounce(() => {
    runBuild({ cwd: ROOT })
        .catch((err) => {
            console.error('build failed:', err.message);
        })
        .finally(() => {
            sseHub.broadcast('reload');
        });
}, REBUILD_DEBOUNCE_MS);

const watcher = watchPaths(WATCH_ROOTS, WATCH_EXTENSIONS, (filename) => {
    console.log(`change detected: ${filename}`);
    rebuild();
});

server.listen(PORT, () => {
    console.log(`Serving at http://localhost:${PORT}`);
    runBuild({ cwd: ROOT }).catch((err) => {
        console.error('initial build failed:', err.message);
    });
});

process.on('SIGINT', () => {
    watcher.close();
    server.close(() => process.exit(0));
});
