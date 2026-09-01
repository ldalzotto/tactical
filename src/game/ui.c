#include "ui.h"
#include "lib/linkage.h"

PUBLIC bool point_in_rect(rect_t r, int x, int y) {
    return x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height;
}
