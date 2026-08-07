function createSSEHub() {
    const clients = new Set();

    return {
        handle(req, res) {
            res.writeHead(200, {
                'Content-Type': 'text/event-stream',
                'Cache-Control': 'no-cache',
                Connection: 'keep-alive',
            });
            clients.add(res);
            req.on('close', () => clients.delete(res));
        },

        broadcast(event, data = {}) {
            const payload = `event: ${event}\ndata: ${JSON.stringify(data)}\n\n`;
            for (const res of clients) {
                res.write(payload);
            }
        },

        get size() {
            return clients.size;
        },
    };
}

module.exports = { createSSEHub };
