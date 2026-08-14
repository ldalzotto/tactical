#include "assert.h"

#ifdef APP_BUILD_TESTS
static bool g_expect_panic = false;
static bool g_panic_occurred = false;
static bool g_expect_trap = false;
static bool g_trap_occurred = false;
#endif

PUBLIC void panic(bool condition) {
    if (!condition) {
#ifdef APP_BUILD_TESTS
        if (g_expect_panic) {
            g_panic_occurred = true;
            return;
        }
        g_trap_occurred = true;
#endif
        __builtin_trap();
    }
}

#ifdef APP_BUILD_TESTS
PUBLIC void expect_panic_begin(void) {
    g_expect_panic = true;
    g_panic_occurred = false;
}

PUBLIC bool expect_panic_end(void) {
    g_expect_panic = false;
    bool occurred = g_panic_occurred;
    g_panic_occurred = false;
    return occurred;
}

PUBLIC void expect_trap_begin(void) {
    g_expect_trap = true;
    g_trap_occurred = false;
}

PUBLIC bool expect_trap_end(void) {
    bool expected = g_expect_trap;
    bool occurred = g_trap_occurred;
    g_expect_trap = false;
    g_trap_occurred = false;
    return expected && occurred;
}
#endif
