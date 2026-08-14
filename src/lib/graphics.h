#pragma once

#include "linkage.h"

#include <stdint.h>
#include <stdbool.h>

#include "memory.h"

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} rgba_t;

SLICE_DEFINE(rgba_t);

bool rgba_equals(rgba_t a, rgba_t b);

PUBLIC void graphics_draw_rectangle(slice_rgba_t framebuffer, int fb_width, int x, int y, int width, int height, rgba_t color);

// Fakes transparency: checkerboard fill (on absolute framebuffer
// coordinates, so the pattern doesn't shift with the rect) instead of alpha
// blending, which this module doesn't support.
PUBLIC void graphics_draw_rectangle_dithered(slice_rgba_t framebuffer, int fb_width, int x, int y, int width, int height, rgba_t color);

#ifdef APP_UNITY_BUILD
#include "graphics.c"
#endif
