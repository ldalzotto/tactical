#include "assert.h"

#ifdef APP_BUILD_TESTS
static bool g_expect_panic = false;
static bool g_panic_occurred = false;
static bool g_expect_trap = false;
static bool g_trap_occurred = false;
#endif

__attribute__((import_module("env"), import_name("report_panic")))
extern void __report_panic(void *file_begin, void *file_end, int line, void *msg_begin, void *msg_end);

PUBLIC void panic_at(bool condition, slice_t file, int line, slice_t msg) {
    if (!condition) {
#ifdef APP_BUILD_TESTS
        if (g_expect_panic) {
            g_panic_occurred = true;
            return;
        }
        g_trap_occurred = true;
#endif
        // __report_panic always throws on the JS side, which unwinds the
        // wasm call immediately -- control never returns here, 
        // in theory the __builtin_trap is never called, but we keep it for safety.
        __report_panic(file.begin, file.end, line, msg.begin, msg.end);
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
