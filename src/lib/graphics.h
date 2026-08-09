#pragma once

#include <stdint.h>

#include "memory.h"

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} rgba_t;

SLICE_DEFINE(rgba_t);

void graphics_draw_rectangle(slice_rgba_t framebuffer, int fb_width, int x, int y, int width, int height, rgba_t color);
