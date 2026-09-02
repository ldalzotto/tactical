#include "test_fmt.h"
#include "lib/assert.h"
#include "lib/fmt.h"
#include "lib/linkage.h"
#include "lib/memory.h"
#include "test.h"
#include <stdbool.h>
#include <stdint.h>

PRIVATE void test_fmt_uint_to_chars_zero(linear_allocator_t *allocator) {
    (void)allocator;
    char buf[10];
    slice_t chars = fmt_uint_to_chars(0, (slice_t){ .begin = buf, .end = buf + sizeof(buf) });
    assert_test(slice_equals(chars, STR("0")));
}

PRIVATE void test_fmt_uint_to_chars_multi_digit(linear_allocator_t *allocator) {
    (void)allocator;
    char buf[10];
    slice_t chars = fmt_uint_to_chars(123, (slice_t){ .begin = buf, .end = buf + sizeof(buf) });
    assert_test(slice_equals(chars, STR("123")));
}

PRIVATE void test_fmt_uint_to_chars_max(linear_allocator_t *allocator) {
    (void)allocator;
    char buf[10];
    slice_t chars = fmt_uint_to_chars(UINT32_MAX, (slice_t){ .begin = buf, .end = buf + sizeof(buf) });
    assert_test(slice_equals(chars, STR("4294967295")));
}

PRIVATE void test_fmt_int_to_chars_zero(linear_allocator_t *allocator) {
    (void)allocator;
    char buf[11];
    slice_t chars = fmt_int_to_chars(0, (slice_t){ .begin = buf, .end = buf + sizeof(buf) });
    assert_test(slice_equals(chars, STR("0")));
}

PRIVATE void test_fmt_int_to_chars_positive(linear_allocator_t *allocator) {
    (void)allocator;
    char buf[11];
    slice_t chars = fmt_int_to_chars(42, (slice_t){ .begin = buf, .end = buf + sizeof(buf) });
    assert_test(slice_equals(chars, STR("42")));
}

PRIVATE void test_fmt_int_to_chars_negative(linear_allocator_t *allocator) {
    (void)allocator;
    char buf[11];
    slice_t chars = fmt_int_to_chars(-42, (slice_t){ .begin = buf, .end = buf + sizeof(buf) });
    assert_test(slice_equals(chars, STR("-42")));
}

PRIVATE void test_fmt_int_to_chars_int32_min(linear_allocator_t *allocator) {
    (void)allocator;
    char buf[11];
    slice_t chars = fmt_int_to_chars(INT32_MIN, (slice_t){ .begin = buf, .end = buf + sizeof(buf) });
    assert_test(slice_equals(chars, STR("-2147483648")));
}

const test_case_t g_fmt_tests[] = {
    { TEST_NAME("fmt_uint_to_chars_zero"), test_fmt_uint_to_chars_zero },
    { TEST_NAME("fmt_uint_to_chars_multi_digit"), test_fmt_uint_to_chars_multi_digit },
    { TEST_NAME("fmt_uint_to_chars_max"), test_fmt_uint_to_chars_max },
    { TEST_NAME("fmt_int_to_chars_zero"), test_fmt_int_to_chars_zero },
    { TEST_NAME("fmt_int_to_chars_positive"), test_fmt_int_to_chars_positive },
    { TEST_NAME("fmt_int_to_chars_negative"), test_fmt_int_to_chars_negative },
    { TEST_NAME("fmt_int_to_chars_int32_min"), test_fmt_int_to_chars_int32_min },
};

const uint32_t g_fmt_tests_count = sizeof(g_fmt_tests) / sizeof(g_fmt_tests[0]);
