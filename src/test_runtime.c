#include "test_runtime.h"
#include "lib/assert.h"
#include "lib/runtime.h"

static void test_pass_example(void) {
    assert_test(1 + 1 == 2);
}

static void test_fail_example(void) {
    expect_panic_begin();
    assert_test(1 + 1 == 3);
    assert_test(expect_panic_end());

    expect_panic_begin();
    assert_test(1 == 2);
    assert_test(expect_panic_end());

    assert_test(1 + 1 == 2);
}

static void test_input_event_layout(void) {
    assert_test(sizeof(input_event_t) == 12);

    static input_event_t events[2];
    slice_input_event_t s = { .slice = { events, events + 2 } };

    SLICE_AT(s, 0) = (input_event_t){ .type = INPUT_EVENT_MOUSE_MOVE, .x = 1, .y = 2 };
    SLICE_AT(s, 1) = (input_event_t){ .type = INPUT_EVENT_MOUSE_CLICK, .x = 3, .y = 4 };

    assert_test(SLICE_AT(s, 0).type == INPUT_EVENT_MOUSE_MOVE);
    assert_test(SLICE_AT(s, 1).x == 3);
    assert_test(SLICE_AT(s, 1).y == 4);
}

const test_case_t g_runtime_tests[] = {
    { TEST_NAME("pass_example"), test_pass_example },
    { TEST_NAME("fail_example"), test_fail_example },
    { TEST_NAME("input_event_layout"), test_input_event_layout },
};

const uint32_t g_runtime_tests_count = sizeof(g_runtime_tests) / sizeof(g_runtime_tests[0]);
