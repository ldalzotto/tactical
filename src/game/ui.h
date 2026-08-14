#pragma once

#include "../lib/linkage.h"
#include "../lib/memory.h"

#include <stdbool.h>

typedef struct {
    int x, y, width, height;
} rect_t;
SLICE_DEFINE(rect_t);

PUBLIC bool point_in_rect(rect_t r, int x, int y);

#ifdef APP_UNITY_BUILD
#include "ui.c"
#endif
