#pragma once

#include "../lib/linkage.h"

#include <stdbool.h>

#include "position.h"

// Bresenham line-walking state, stepping one axis (or both, on a diagonal
// tie) per iteration from `from` towards a `to` supplied at each `next`
// call. Pure math.
typedef struct {
    int abs_dx;
    int step_x;
    int abs_dy;
    int step_y;
    int x;
    int y;
    int error;
    bool alt_tie_break;
} geometry_line_iter_t;

// `alt_tie_break` picks which of the two adjacent tiles a diagonal tie
// steps through (err2 landing exactly on -abs_dy or abs_dx): false takes
// the standard Bresenham path, true takes the other one. Same endpoints,
// different tiles in between -- e.g. pathing.c ORs both to make line-of-sight
// permissive about corners a single path would clip.
PUBLIC geometry_line_iter_t geometry_line_iter_start(position_t from, position_t to, bool alt_tie_break);

// Advances the iterator to the next tile on the ray and writes it to
// `out_tile`. Returns false once `to` is reached, without writing `to`
// itself -- so both endpoints are excluded from the walked tiles.
PUBLIC bool geometry_line_iter_next(geometry_line_iter_t *it, position_t to, position_t *out_tile);

#ifdef APP_UNITY_BUILD
#include "geometry.c"
#endif
