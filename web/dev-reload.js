new EventSource('/__reload').addEventListener('reload', () => {
    location.reload();
});
