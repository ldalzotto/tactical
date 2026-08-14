#include <stddef.h>
#include <stdint.h>

// Minimal freestanding implementations of the libc memory functions the app
// relies on. There is no libc in this wasm build (-nostdlib), and neither
// Emscripten's nor compiler-rt's builtins are available to provide these.

void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;

    if (d == s || n == 0) {
        return dest;
    }

    if (d < s) {
        while (n--) {
            *d++ = *s++;
        }
    } else {
        d += n;
        s += n;
        while (n--) {
            *--d = *--s;
        }
    }

    return dest;
}

void *memset(void *dest, int c, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    while (n--) {
        *d++ = (uint8_t)c;
    }
    return dest;
}
