#include "geometry.h"

PUBLIC geometry_line_iter_t geometry_line_iter_start(position_t from, position_t to) {
    int dx = to.x - from.x;
    int dy = to.y - from.y;

    return (geometry_line_iter_t) {
        .adx = dx < 0 ? -dx : dx,
        .sx = dx < 0 ? -1 : 1,
        .ady = dy < 0 ? -dy : dy,
        .sy = dy < 0 ? -1 : 1,
        .x = from.x,
        .y = from.y,
        .err = (dx < 0 ? -dx : dx) - (dy < 0 ? -dy : dy),
    };
}

PUBLIC bool geometry_line_iter_next(geometry_line_iter_t *it, position_t to, position_t *out_tile) {
    int err2 = it->err * 2;
    if (err2 > -it->ady) {
        it->err -= it->ady;
        it->x += it->sx;
    }
    if (err2 < it->adx) {
        it->err += it->adx;
        it->y += it->sy;
    }

    if (it->x == to.x && it->y == to.y) {
        return false;
    }

    *out_tile = (position_t) { it->x, it->y };
    return true;
}
