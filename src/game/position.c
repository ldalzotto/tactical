#include "position.h"

position_t position_add(position_t a, position_t b) {
    return (position_t){ a.x + b.x, a.y + b.y };
}

position_t position_sub(position_t a, position_t b) {
    return (position_t){ a.x - b.x, a.y - b.y };
}

bool position_equals(position_t a, position_t b) {
    return a.x == b.x && a.y == b.y;
}
