#include "test_runtime.h"
#include "lib/assert.h"
#include "lib/linkage.h"
#include "lib/memory.h"
#include "lib/runtime.h"
#include "test.h"
#include <stdint.h>

PRIVATE void test_pass_example(linear_allocator_t *allocator) {
    (void)allocator;
    assert_test(1 + 1 == 2);
}

PRIVATE void test_fail_example(linear_allocator_t *allocator) {
    (void)allocator;
    expect_panic_begin();
    assert_test(1 + 1 == 3);
    assert_test(expect_panic_end());

    expect_panic_begin();
    assert_test(1 == 2);
    assert_test(expect_panic_end());

    assert_test(1 + 1 == 2);
}

PRIVATE void test_test_discovery_out_of_range_panics(linear_allocator_t *allocator) {
    (void)allocator;

    // test_lookup's fallback path (assert_test(false)) is only reachable
    // through an out-of-range discovery index.
    expect_panic_begin();
    const char *name = test_discovery_name_begin(0xFFFFFFFF);
    assert_test(expect_panic_end());
    assert_test(name == 0);
}

PRIVATE void test_panic_without_expect_panic_traps(linear_allocator_t *allocator) {
    (void)allocator;

    // panic's non-expect_panic path falls through to report_panic, which
    // throws on the JS side and unwinds the wasm instance. The JS runner
    // treats this test's unwind as the expected outcome and confirms it
    // via test_expect_trap_end.
    expect_trap_begin();
    panic(false);
}

PRIVATE void test_expect_trap_end_without_trap_reports_false(linear_allocator_t *allocator) {
    (void)allocator;

    // The expected-but-no-trap side of expect_trap_end's `&&` condition:
    // a test can ask for a trap and then finish without reaching one.
    expect_trap_begin();
    assert_test(!expect_trap_end());
}

PRIVATE void test_input_event_layout(linear_allocator_t *allocator) {
    assert_test(sizeof(input_event_t) == 12);

    slice_input_event_t s = LINEAR_ALLOCATOR_PUSH(allocator, s, 2);

    SLICE_AT(s, 0) = (input_event_t){ .type = INPUT_EVENT_MOUSE_MOVE, .x = 1, .y = 2 };
    SLICE_AT(s, 1) = (input_event_t){ .type = INPUT_EVENT_MOUSE_CLICK, .x = 3, .y = 4 };

    assert_test(SLICE_AT(s, 0).type == INPUT_EVENT_MOUSE_MOVE);
    assert_test(SLICE_AT(s, 1).x == 3);
    assert_test(SLICE_AT(s, 1).y == 4);

    LINEAR_ALLOCATOR_POP(allocator, s);
}

const test_case_t g_runtime_tests[] = {
    { TEST_NAME("pass_example"), test_pass_example },
    { TEST_NAME("fail_example"), test_fail_example },
    { TEST_NAME("test_discovery_out_of_range_panics"), test_test_discovery_out_of_range_panics },
    { TEST_NAME("panic_without_expect_panic_traps"), test_panic_without_expect_panic_traps },
    { TEST_NAME("expect_trap_end_without_trap_reports_false"), test_expect_trap_end_without_trap_reports_false },
    { TEST_NAME("input_event_layout"), test_input_event_layout },
};

const uint32_t g_runtime_tests_count = sizeof(g_runtime_tests) / sizeof(g_runtime_tests[0]);
