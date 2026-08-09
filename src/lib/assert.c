#include "assert.h"

#ifdef APP_BUILD_TESTS
static bool g_expect_panic = false;
static bool g_panic_occurred = false;
#endif

void panic(bool condition) {
    if (!condition) {
#ifdef APP_BUILD_TESTS
        if (g_expect_panic) {
            g_panic_occurred = true;
            return;
        }
#endif
        __builtin_trap();
    }
}

#ifdef APP_BUILD_TESTS
void expect_panic_begin(void) {
    g_expect_panic = true;
    g_panic_occurred = false;
}

bool expect_panic_end(void) {
    g_expect_panic = false;
    bool occurred = g_panic_occurred;
    g_panic_occurred = false;
    return occurred;
}
#endif
