#pragma once

#include "linkage.h"

#include <stdint.h>

#include "memory.h"

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} rgba_t;

SLICE_DEFINE(rgba_t);

PUBLIC void graphics_draw_rectangle(slice_rgba_t framebuffer, int fb_width, int x, int y, int width, int height, rgba_t color);

// "Fake transparent" fill: same opaque overwrite as graphics_draw_rectangle,
// but skips every other pixel (checkerboard on absolute framebuffer
// coordinates, so the pattern doesn't shift with the rect's position) --
// whatever was drawn earlier at the skipped pixels shows through. There's no
// alpha blending anywhere in this module (confirmed: graphics_draw_rectangle
// is a flat SLICE_AT(...) = color overwrite), so this stipple is the only
// way to fake transparency.
PUBLIC void graphics_draw_rectangle_dithered(slice_rgba_t framebuffer, int fb_width, int x, int y, int width, int height, rgba_t color);

#ifdef APP_UNITY_BUILD
#include "graphics.c"
#endif
