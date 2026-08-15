#pragma once

#include "../lib/linkage.h"

#include <stdbool.h>

#include "entity.h"
#include "grid.h"
#include "position.h"

// True if the straight ray from `from` to `to` (`to` != `from`) is
// unobstructed: every intermediate tile -- both endpoints excluded, so
// neither `from`'s nor `to`'s own tile can block sight to itself -- is
// walkable and unoccupied. Standard Bresenham line, stepping one axis (or
// both, on a diagonal tie) per iteration.
PUBLIC bool geometry_line_of_sight_clear(grid_t grid, slice_entity_t entities, position_t from, position_t to);

#ifdef APP_UNITY_BUILD
#include "geometry.c"
#endif
