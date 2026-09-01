#include "test_fmt.h"
#include "lib/assert.h"
#include "lib/fmt.h"
#include "lib/linkage.h"
#include "lib/memory.h"
#include "test.h"
#include <stdbool.h>
#include <stdint.h>

// No libc/string.h in this build, so compare buffers by hand against a
// STR() literal instead of memcmp.
PRIVATE bool chars_equal(const char *buf, int count, slice_t expected) {
    if ((ptrdiff_t)count != bytesize(expected.begin, expected.end)) {
        return false;
    }
    const char *expected_chars = expected.begin;
    for (int i = 0; i < count; i++) {
        if (buf[i] != expected_chars[i]) {
            return false;
        }
    }
    return true;
}

PRIVATE void test_fmt_uint_to_chars_zero(linear_allocator_t *allocator) {
    (void)allocator;
    char buf[10];
    int count = fmt_uint_to_chars(0, buf);
    assert_test(chars_equal(buf, count, STR("0")));
}

PRIVATE void test_fmt_uint_to_chars_multi_digit(linear_allocator_t *allocator) {
    (void)allocator;
    char buf[10];
    int count = fmt_uint_to_chars(123, buf);
    assert_test(chars_equal(buf, count, STR("123")));
}

PRIVATE void test_fmt_uint_to_chars_max(linear_allocator_t *allocator) {
    (void)allocator;
    char buf[10];
    int count = fmt_uint_to_chars(UINT32_MAX, buf);
    assert_test(chars_equal(buf, count, STR("4294967295")));
}

PRIVATE void test_fmt_int_to_chars_zero(linear_allocator_t *allocator) {
    (void)allocator;
    char buf[11];
    int count = fmt_int_to_chars(0, buf);
    assert_test(chars_equal(buf, count, STR("0")));
}

PRIVATE void test_fmt_int_to_chars_positive(linear_allocator_t *allocator) {
    (void)allocator;
    char buf[11];
    int count = fmt_int_to_chars(42, buf);
    assert_test(chars_equal(buf, count, STR("42")));
}

PRIVATE void test_fmt_int_to_chars_negative(linear_allocator_t *allocator) {
    (void)allocator;
    char buf[11];
    int count = fmt_int_to_chars(-42, buf);
    assert_test(chars_equal(buf, count, STR("-42")));
}

PRIVATE void test_fmt_int_to_chars_int32_min(linear_allocator_t *allocator) {
    (void)allocator;
    char buf[11];
    int count = fmt_int_to_chars(INT32_MIN, buf);
    assert_test(chars_equal(buf, count, STR("-2147483648")));
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
