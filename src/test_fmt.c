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

PRIVATE void test_fmt_write_uint(linear_allocator_t *allocator) {
    void *mark = allocator->cursor;
    fmt_write_uint(allocator, 123);
    slice_t captured = { mark, allocator->cursor };

    assert_test(slice_equals(captured, STR("123")));

    linear_allocator_pop(allocator, captured);
}

PRIVATE void test_fmt_write_bool_false(linear_allocator_t *allocator) {
    void *mark = allocator->cursor;
    fmt_write_bool(allocator, false);
    slice_t captured = { mark, allocator->cursor };

    assert_test(slice_equals(captured, STR("false")));

    linear_allocator_pop(allocator, captured);
}

// dest == 0 routes through the runtime debug bridge (write/flush_line)
// instead of an allocator -- exercised here just to cover that path,
// since there's no allocator buffer to assert the output against.
PRIVATE void test_fmt_write_null_dest(linear_allocator_t *allocator) {
    (void)allocator;
    fmt_write(0, STR("hello"));
}

PRIVATE void test_fmt_end_line_null_dest(linear_allocator_t *allocator) {
    (void)allocator;
    fmt_end_line(0);
}

const test_case_t g_fmt_tests[] = {
    { TEST_NAME("fmt_uint_to_chars_zero"), test_fmt_uint_to_chars_zero },
    { TEST_NAME("fmt_uint_to_chars_multi_digit"), test_fmt_uint_to_chars_multi_digit },
    { TEST_NAME("fmt_uint_to_chars_max"), test_fmt_uint_to_chars_max },
    { TEST_NAME("fmt_int_to_chars_zero"), test_fmt_int_to_chars_zero },
    { TEST_NAME("fmt_int_to_chars_positive"), test_fmt_int_to_chars_positive },
    { TEST_NAME("fmt_int_to_chars_negative"), test_fmt_int_to_chars_negative },
    { TEST_NAME("fmt_int_to_chars_int32_min"), test_fmt_int_to_chars_int32_min },
    { TEST_NAME("fmt_write_uint"), test_fmt_write_uint },
    { TEST_NAME("fmt_write_bool_false"), test_fmt_write_bool_false },
    { TEST_NAME("fmt_write_null_dest"), test_fmt_write_null_dest },
    { TEST_NAME("fmt_end_line_null_dest"), test_fmt_end_line_null_dest },
};

const uint32_t g_fmt_tests_count = sizeof(g_fmt_tests) / sizeof(g_fmt_tests[0]);
