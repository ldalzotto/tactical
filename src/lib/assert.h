#pragma once

#include <stdbool.h>

void panic(bool condition);

#ifdef APP_BUILD_TESTS
// Marks a region where a failing assert_test/panic should be swallowed
// instead of trapping. Can be called multiple times per test; each
// expect_panic_begin must be paired with an expect_panic_end, which
// reports whether a panic actually occurred during the region.
void expect_panic_begin(void);
bool expect_panic_end(void);
#endif

#ifndef NDEBUG
#define assert_debug panic
#else
#define assert_debug(...) ((void)0)
#endif

static inline void assert_test(bool condition) {
    panic(condition);
}