#include <stdint.h>

#define FB_WIDTH 320
#define FB_HEIGHT 240

static uint8_t framebuffer[FB_WIDTH * FB_HEIGHT * 4];

static float elapsed_ms = 0.0f;

__attribute__((export_name("get_framebuffer")))
uint8_t *get_framebuffer(void) {
    return framebuffer;
}

__attribute__((export_name("get_width")))
int get_width(void) {
    return FB_WIDTH;
}

__attribute__((export_name("get_height")))
int get_height(void) {
    return FB_HEIGHT;
}

__attribute__((export_name("init")))
void init(void) {
    elapsed_ms = 0.0f;
}

__attribute__((export_name("frame")))
void frame(float dt_ms) {
    elapsed_ms += dt_ms;

    int shift = (int)(elapsed_ms * 0.05f);

    for (int y = 0; y < FB_HEIGHT; y++) {
        for (int x = 0; x < FB_WIDTH; x++) {
            int idx = (y * FB_WIDTH + x) * 4;
            framebuffer[idx + 0] = (uint8_t)(x + shift);
            framebuffer[idx + 1] = (uint8_t)(y - shift);
            framebuffer[idx + 2] = (uint8_t)((x + y) / 2 + shift);
            framebuffer[idx + 3] = 255;
        }
    }
}
