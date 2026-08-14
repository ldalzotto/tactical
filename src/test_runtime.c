#include "test_runtime.h"
#include "lib/assert.h"
#include "lib/runtime.h"

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
    { TEST_NAME("input_event_layout"), test_input_event_layout },
};

const uint32_t g_runtime_tests_count = sizeof(g_runtime_tests) / sizeof(g_runtime_tests[0]);
