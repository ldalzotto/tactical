#include "test_memory.h"
#include "lib/assert.h"
#include "lib/runtime.h"

SLICE_DEFINE(uint8_t);
SLICE_DEFINE(uint32_t);

PRIVATE void test_byteoffset(void) {
    static uint32_t values[4] = { 10, 20, 30, 40 };

    uint32_t *third = typeoffset(values, 2);

    assert_test(*third == 30);
}

PRIVATE void test_linear_allocator_init(void) {
    static char buffer[16];
    slice_t data = { buffer, buffer + sizeof(buffer) };

    linear_allocator_t allocator = linear_allocator_init(data);

    assert_test(allocator.cursor == data.begin);
    assert_test(allocator.data.begin == data.begin);
    assert_test(allocator.data.end == data.end);
}

PRIVATE void test_linear_allocator_deinit(void) {
    static char buffer[16];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    linear_allocator_deinit(&allocator);

    assert_test(allocator.cursor == allocator.data.begin);
}

PRIVATE void test_linear_allocator_deinit_panics_on_leftover_allocation(void) {
    static char buffer[16];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_uint8_t bytes = LINEAR_ALLOCATOR_PUSH(&allocator, bytes, 4);
    (void)bytes;

    expect_panic_begin();
    linear_allocator_deinit(&allocator);
    assert_test(expect_panic_end());
}

PRIVATE void test_linear_allocator_push(void) {
    static char buffer[16];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_uint8_t bytes = LINEAR_ALLOCATOR_PUSH(&allocator, bytes, 4);

    assert_test(bytes.begin == (uint8_t *)buffer);
    assert_test(SLICE_BYTESIZE(bytes) == 4);
    assert_test(allocator.cursor == bytes.end);

    LINEAR_ALLOCATOR_POP(&allocator, bytes);
}

PRIVATE void test_linear_allocator_push_panics_on_overflow(void) {
    static char buffer[4];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    expect_panic_begin();
    linear_allocator_push(&allocator, 8);
    assert_test(expect_panic_end());
}

PRIVATE void test_linear_allocator_push_alignment(void) {
    static _Alignas(8) char buffer[16];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_uint8_t byte = LINEAR_ALLOCATOR_PUSH(&allocator, byte, 1);

    slice_uint32_t witness;
    slice_t padding = LINEAR_ALLOCATOR_PUSH_ALIGNMENT(&allocator, witness);
    assert_test(SLICE_BYTESIZE(padding) == _Alignof(uint32_t) - 1);

    slice_uint32_t value = LINEAR_ALLOCATOR_PUSH(&allocator, value, 1);
    assert_test((uintptr_t)value.begin % _Alignof(uint32_t) == 0);

    LINEAR_ALLOCATOR_POP(&allocator, value);
    linear_allocator_pop(&allocator, padding);
    LINEAR_ALLOCATOR_POP(&allocator, byte);
}

PRIVATE void test_linear_allocator_push_alignment_panics_on_non_power_of_two_alignment(void) {
    static char buffer[16];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    expect_panic_begin();
    linear_allocator_push_alignment(&allocator, 3);
    assert_test(expect_panic_end());
}

PRIVATE void test_linear_allocator_pop(void) {
    static char buffer[16];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_uint8_t bytes = LINEAR_ALLOCATOR_PUSH(&allocator, bytes, 8);

    LINEAR_ALLOCATOR_POP(&allocator, bytes);

    assert_test(allocator.cursor == allocator.data.begin);
}

PRIVATE void test_linear_allocator_pop_panics_on_marker_before_data_begin(void) {
    static char buffer[16];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_t marker = { byteoffset(data.begin, -1), allocator.cursor };

    expect_panic_begin();
    linear_allocator_pop(&allocator, marker);
    assert_test(expect_panic_end());
}

PRIVATE void test_linear_allocator_pop_panics_on_marker_end_mismatch(void) {
    static char buffer[16];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_uint8_t bytes = LINEAR_ALLOCATOR_PUSH(&allocator, bytes, 8);
    slice_t marker = { bytes.begin, byteoffset(bytes.end, -1) };

    expect_panic_begin();
    linear_allocator_pop(&allocator, marker);
    assert_test(expect_panic_end());
}

PRIVATE void test_linear_allocator_pop_move(void) {
    static char buffer[64];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_uint8_t a = LINEAR_ALLOCATOR_PUSH(&allocator, a, 4);
    SLICE_AT(a, 0) = 'A'; SLICE_AT(a, 1) = 'A';
    SLICE_AT(a, 2) = 'A'; SLICE_AT(a, 3) = 'A';

    slice_uint8_t b = LINEAR_ALLOCATOR_PUSH(&allocator, b, 4);

    slice_uint8_t c = LINEAR_ALLOCATOR_PUSH(&allocator, c, 4);
    SLICE_AT(c, 0) = 'C'; SLICE_AT(c, 1) = 'C';
    SLICE_AT(c, 2) = 'C'; SLICE_AT(c, 3) = 'C';

    LINEAR_ALLOCATOR_POP_MOVE(&allocator, c, b);

    assert_test(SLICE_AT(b, 0) == 'C');
    assert_test(SLICE_AT(b, 1) == 'C');
    assert_test(SLICE_AT(b, 2) == 'C');
    assert_test(SLICE_AT(b, 3) == 'C');
    assert_test(allocator.cursor == byteoffset(b.begin, 4));

    linear_allocator_pop(&allocator, (slice_t){ b.begin, allocator.cursor });
    linear_allocator_pop(&allocator, a.slice);
}

PRIVATE void test_linear_allocator_pop_move_panics_on_move_forward(void) {
    static char buffer[64];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_uint8_t a = LINEAR_ALLOCATOR_PUSH(&allocator, a, 4);
    slice_uint8_t b = LINEAR_ALLOCATOR_PUSH(&allocator, b, 4);
    (void)a;

    slice_t to = { byteoffset(b.begin, 4), byteoffset(b.begin, 8) };

    expect_panic_begin();
    linear_allocator_pop_move(&allocator, b.slice, to);
    assert_test(expect_panic_end());
}

PRIVATE void test_linear_allocator_pop_move_panics_on_from_not_top_of_stack(void) {
    static char buffer[64];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_uint8_t a = LINEAR_ALLOCATOR_PUSH(&allocator, a, 4);
    slice_uint8_t b = LINEAR_ALLOCATOR_PUSH(&allocator, b, 4);
    (void)b;

    slice_t to = { a.begin, a.begin };

    expect_panic_begin();
    linear_allocator_pop_move(&allocator, a.slice, to);
    assert_test(expect_panic_end());
}

PRIVATE void test_slice_at(void) {
    static uint8_t buffer[4] = { 1, 2, 3, 4 };
    slice_uint8_t s = { .slice = { buffer, buffer + sizeof(buffer) } };

    assert_test(SLICE_AT(s, 0) == 1);
    assert_test(SLICE_AT(s, 2) == 3);

    SLICE_AT(s, 1) = 42;
    assert_test(buffer[1] == 42);
}

PRIVATE void test_slice_at_panics_on_non_power_of_two_alignment(void) {
    static uint8_t buffer[8];
    slice_t s = { buffer, buffer + sizeof(buffer) };

    expect_panic_begin();
    slice_at(s, 0, 3);
    assert_test(expect_panic_end());
}

PRIVATE void test_slice_at_panics_on_out_of_bounds(void) {
    static uint8_t buffer[4];
    slice_t s = { buffer, buffer + sizeof(buffer) };

    expect_panic_begin();
    slice_at(s, 8, 1);
    assert_test(expect_panic_end());
}

PRIVATE void test_slice_at_panics_on_misalignment(void) {
    static _Alignas(4) uint8_t buffer[8];
    slice_t s = { buffer, buffer + sizeof(buffer) };

    expect_panic_begin();
    slice_at(s, 1, 4);
    assert_test(expect_panic_end());
}

PRIVATE void test_slice_advance(void) {
    static uint8_t buffer[4] = { 1, 2, 3, 4 };
    slice_uint8_t s = { .slice = { buffer, buffer + sizeof(buffer) } };

    slice_uint8_t advanced = SLICE_ADVANCE(s, 2);

    assert_test(advanced.begin == buffer + 2);
    assert_test(advanced.end == s.end);
    assert_test(SLICE_AT(advanced, 0) == 3);
}

PRIVATE void test_slice_advance_panics_on_out_of_bounds(void) {
    static uint8_t buffer[4];
    slice_uint8_t s = { .slice = { buffer, buffer + sizeof(buffer) } };

    expect_panic_begin();
    (void)SLICE_ADVANCE(s, 8);
    assert_test(expect_panic_end());
}

PRIVATE void test_bytesize(void) {
    static uint8_t buffer[7];
    slice_uint8_t s = { .slice = { buffer, buffer + sizeof(buffer) } };

    assert_test(SLICE_BYTESIZE(s) == 7);
}

const test_case_t g_memory_tests[] = {
    { TEST_NAME("byteoffset"), test_byteoffset },
    { TEST_NAME("linear_allocator_init"), test_linear_allocator_init },
    { TEST_NAME("linear_allocator_deinit"), test_linear_allocator_deinit },
    { TEST_NAME("linear_allocator_deinit_panics_on_leftover_allocation"), test_linear_allocator_deinit_panics_on_leftover_allocation },
    { TEST_NAME("linear_allocator_push"), test_linear_allocator_push },
    { TEST_NAME("linear_allocator_push_panics_on_overflow"), test_linear_allocator_push_panics_on_overflow },
    { TEST_NAME("linear_allocator_push_alignment"), test_linear_allocator_push_alignment },
    { TEST_NAME("linear_allocator_push_alignment_panics_on_non_power_of_two_alignment"), test_linear_allocator_push_alignment_panics_on_non_power_of_two_alignment },
    { TEST_NAME("linear_allocator_pop"), test_linear_allocator_pop },
    { TEST_NAME("linear_allocator_pop_panics_on_marker_before_data_begin"), test_linear_allocator_pop_panics_on_marker_before_data_begin },
    { TEST_NAME("linear_allocator_pop_panics_on_marker_end_mismatch"), test_linear_allocator_pop_panics_on_marker_end_mismatch },
    { TEST_NAME("linear_allocator_pop_move"), test_linear_allocator_pop_move },
    { TEST_NAME("linear_allocator_pop_move_panics_on_move_forward"), test_linear_allocator_pop_move_panics_on_move_forward },
    { TEST_NAME("linear_allocator_pop_move_panics_on_from_not_top_of_stack"), test_linear_allocator_pop_move_panics_on_from_not_top_of_stack },
    { TEST_NAME("slice_at"), test_slice_at },
    { TEST_NAME("slice_at_panics_on_non_power_of_two_alignment"), test_slice_at_panics_on_non_power_of_two_alignment },
    { TEST_NAME("slice_at_panics_on_out_of_bounds"), test_slice_at_panics_on_out_of_bounds },
    { TEST_NAME("slice_at_panics_on_misalignment"), test_slice_at_panics_on_misalignment },
    { TEST_NAME("slice_advance"), test_slice_advance },
    { TEST_NAME("slice_advance_panics_on_out_of_bounds"), test_slice_advance_panics_on_out_of_bounds },
    { TEST_NAME("bytesize"), test_bytesize },
};

const uint32_t g_memory_tests_count = sizeof(g_memory_tests) / sizeof(g_memory_tests[0]);
