const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');

const PORT = process.env.PORT || 8080;
const ROOT = __dirname;

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

const server = http.createServer((req, res) => {
    const urlPath = decodeURIComponent(req.url.split('?')[0]);
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

server.listen(PORT, () => {
    console.log(`Serving at http://localhost:${PORT}`);
});
