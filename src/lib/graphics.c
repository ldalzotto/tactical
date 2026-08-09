#include "graphics.h"

void graphics_draw_rectangle(slice_rgba_t framebuffer, int fb_width, int x, int y, int width, int height, rgba_t color) {
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            SLICE_AT(framebuffer, (y + j) * fb_width + (x + i)) = color;
        }
    }
}
