#include "position.h"

PUBLIC position_t position_add(position_t a, position_t b) {
    return (position_t){ a.x + b.x, a.y + b.y };
}

PUBLIC bool position_equals(position_t a, position_t b) {
    return a.x == b.x && a.y == b.y;
}

PUBLIC bool position_in_tiles(slice_position_t tiles, position_t position) {
    for (SLICE_FOREACH(tiles, tile_s)) {
        if (position_equals(SLICE_DEREF(tile_s), position)) {
            return true;
        }
    }
    return false;
}
