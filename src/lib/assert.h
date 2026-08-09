#pragma once

#include <stdbool.h>

void panic(bool condition);

#ifndef NDEBUG
#define assert_debug panic
#else
#define assert_debug(...) ((void)0)
#endif

static inline void assert_test(bool condition) {
    panic(condition);
}