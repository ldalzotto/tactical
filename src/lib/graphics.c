#include "graphics.h"

bool rgba_equals(rgba_t a, rgba_t b) {
    return (a.r == b.r) & (a.g == b.g) & (a.b == b.b) & (a.a == b.a);
}

PUBLIC void graphics_draw_rectangle(slice_rgba_t framebuffer, int fb_width, int x, int y, int width, int height, rgba_t color) {
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            SLICE_AT(framebuffer, (y + j) * fb_width + (x + i)) = color;
        }
    }
}

PUBLIC void graphics_draw_rectangle_dithered(slice_rgba_t framebuffer, int fb_width, int x, int y, int width, int height, rgba_t color) {
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            int px = x + i;
            int py = y + j;
            if ((px + py) % 2 != 0) {
                continue;
            }
            SLICE_AT(framebuffer, py * fb_width + px) = color;
        }
    }
}
