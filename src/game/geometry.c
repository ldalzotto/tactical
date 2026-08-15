#include "geometry.h"

PUBLIC bool geometry_line_of_sight_clear(grid_t grid, slice_entity_t entities, position_t from, position_t to) {
    int dx = to.x - from.x;
    int adx = dx < 0 ? -dx : dx;
    int sx = dx < 0 ? -1 : 1;

    int dy = to.y - from.y;
    int ady = dy < 0 ? -dy : dy;
    int sy = dy < 0 ? -1 : 1;

    int x = from.x;
    int y = from.y;
    int err = adx - ady;

    for (;;) {
        int err2 = err * 2;
        if (err2 > -ady) {
            err -= ady;
            x += sx;
        }
        if (err2 < adx) {
            err += adx;
            y += sy;
        }

        if (x == to.x && y == to.y) {
            break;
        }

        position_t tile = { x, y };

        // Obstacles can be a non walkable tile
        if (!grid_is_walkable(grid, tile)) {
            return false;
        }

        // Obstacles can be a blocking entity
        if (entity_find_at(entities, tile) != 0) {
            return false;
        }
    }

    return true;
}
