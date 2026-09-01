#include "geometry.h"
#include "game/position.h"
#include "lib/linkage.h"

PUBLIC geometry_line_iter_t geometry_line_iter_start(position_t from, position_t to) {
    int dx = to.x - from.x;
    int dy = to.y - from.y;

    return (geometry_line_iter_t) {
        .abs_dx = dx < 0 ? -dx : dx,
        .step_x = dx < 0 ? -1 : 1,
        .abs_dy = dy < 0 ? -dy : dy,
        .step_y = dy < 0 ? -1 : 1,
        .x = from.x,
        .y = from.y,
        .error = (dx < 0 ? -dx : dx) - (dy < 0 ? -dy : dy),
    };
}

PUBLIC bool geometry_line_iter_next(geometry_line_iter_t *it, position_t to, position_t *out_tile) {
    int err2 = it->error * 2;
    if (err2 > -it->abs_dy) {
        it->error -= it->abs_dy;
        it->x += it->step_x;
    }
    if (err2 < it->abs_dx) {
        it->error += it->abs_dx;
        it->y += it->step_y;
    }

    if (it->x == to.x && it->y == to.y) {
        return false;
    }

    *out_tile = (position_t) { it->x, it->y };
    return true;
}
