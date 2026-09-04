#include "geometry.h"
#include "game/position.h"
#include "lib/linkage.h"

PUBLIC geometry_line_iter_t geometry_line_iter_start(position_t from, position_t to, bool prefer_y_step) {
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
        .prefer_y_step = prefer_y_step,
    };
}

PUBLIC bool geometry_line_iter_next(geometry_line_iter_t *it, position_t to, position_t *out_tile) {
    int err2 = it->error * 2;
    // On an exact tie (err2 == -abs_dy or == abs_dx), prefer_y_step flips
    // the strict comparison to non-strict, stepping the other axis too --
    // see geometry_line_iter_t's prefer_y_step doc.
    bool step_x = it->prefer_y_step ? (err2 >= -it->abs_dy) : (err2 > -it->abs_dy);
    bool step_y = it->prefer_y_step ? (err2 <= it->abs_dx) : (err2 < it->abs_dx);

    if (step_x) {
        it->error -= it->abs_dy;
        it->x += it->step_x;
    }
    if (step_y) {
        it->error += it->abs_dx;
        it->y += it->step_y;
    }

    if (it->x == to.x && it->y == to.y) {
        return false;
    }

    *out_tile = (position_t) { it->x, it->y };
    return true;
}
