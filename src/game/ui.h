#pragma once

#include <stdbool.h>

typedef struct {
    int x, y, width, height;
} rect_t;

bool point_in_rect(rect_t r, int x, int y);
