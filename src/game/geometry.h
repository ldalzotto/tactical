#pragma once

#include "../lib/linkage.h"

#include <stdbool.h>

#include "position.h"

// Bresenham line-walking state, stepping one axis (or both, on a diagonal
// tie) per iteration from `from` towards a `to` supplied at each `next`
// call. Pure math: no grid, entity, or other game-state awareness.
typedef struct {
    int adx;
    int sx;
    int ady;
    int sy;
    int x;
    int y;
    int err;
} geometry_line_iter_t;

PUBLIC geometry_line_iter_t geometry_line_iter_start(position_t from, position_t to);

// Advances the iterator to the next tile on the ray and writes it to
// `out_tile`. Returns false once `to` is reached, without writing `to`
// itself -- so both endpoints are excluded from the walked tiles.
PUBLIC bool geometry_line_iter_next(geometry_line_iter_t *it, position_t to, position_t *out_tile);

#ifdef APP_UNITY_BUILD
#include "geometry.c"
#endif
