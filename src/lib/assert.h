#pragma once

#include "linkage.h"
#include "memory.h"

#include <stdbool.h>

PUBLIC void panic_at(bool condition, slice_t file, int line, slice_t msg);
#define panic(cond) panic_at((cond), STR(__FILE__), __LINE__, STR(#cond))

#ifdef APP_BUILD_TESTS
// Marks a region where a failing assert_test/panic should be swallowed
// instead of trapping. Can be called multiple times per test; each
// expect_panic_begin must be paired with an expect_panic_end, which
// reports whether a panic actually occurred during the region.
PUBLIC void expect_panic_begin(void);
PUBLIC bool expect_panic_end(void);

// Marks a region where a panic is expected to reach report_panic and
// therefore unwind the wasm instance (report_panic always throws on the JS
// side). The unwind itself cannot be caught in C, so the JS test runner
// calls expect_trap_end after the instance throws to confirm the trap was
// both expected and reached before resetting state.
PUBLIC void expect_trap_begin(void);
PUBLIC bool expect_trap_end(void);
#endif

#ifdef APP_ASSERTIONS
#define assert_debug panic
#else
// Not evaluated (sizeof's operand never runs), but still references the
// condition so disabling assertions can't turn its operands into unused
// parameters/variables and trip -Wunused-*.
#define assert_debug(condition) ((void)sizeof(condition))
#endif

#define assert_test(cond) panic(cond)

#ifdef APP_UNITY_BUILD
#include "assert.c"
#endif
