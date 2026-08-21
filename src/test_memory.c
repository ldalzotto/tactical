#include "test_memory.h"
#include "lib/assert.h"
#include "lib/runtime.h"

PRIVATE void test_byteoffset(linear_allocator_t *allocator) {
    slice_uint32_t values = LINEAR_ALLOCATOR_PUSH(allocator, values, 4);
    SLICE_AT(values, 0) = 10; SLICE_AT(values, 1) = 20;
    SLICE_AT(values, 2) = 30; SLICE_AT(values, 3) = 40;

    uint32_t *third = typeoffset(values.begin, 2);

    assert_test(*third == 30);

    LINEAR_ALLOCATOR_POP(allocator, values);
}

PRIVATE void test_linear_allocator_init(linear_allocator_t *allocator) {
    slice_t data = linear_allocator_push(allocator, 16);

    linear_allocator_t local = linear_allocator_init(data);

    assert_test(local.cursor == data.begin);
    assert_test(local.data.begin == data.begin);
    assert_test(local.data.end == data.end);

    linear_allocator_pop(allocator, data);
}

PRIVATE void test_linear_allocator_deinit(linear_allocator_t *allocator) {
    slice_t data = linear_allocator_push(allocator, 16);
    linear_allocator_t local = linear_allocator_init(data);

    linear_allocator_deinit(&local);

    assert_test(local.cursor == local.data.begin);

    linear_allocator_pop(allocator, data);
}

#ifdef APP_ASSERTIONS
PRIVATE void test_linear_allocator_deinit_panics_on_leftover_allocation(linear_allocator_t *allocator) {
    slice_t data = linear_allocator_push(allocator, 16);
    linear_allocator_t local = linear_allocator_init(data);

    slice_uint8_t bytes = LINEAR_ALLOCATOR_PUSH(&local, bytes, 4);
    (void)bytes;

    expect_panic_begin();
    linear_allocator_deinit(&local);
    assert_test(expect_panic_end());

    linear_allocator_pop(allocator, data);
}
#endif

PRIVATE void test_linear_allocator_push(linear_allocator_t *allocator) {
    slice_t data = linear_allocator_push(allocator, 16);
    linear_allocator_t local = linear_allocator_init(data);

    slice_uint8_t bytes = LINEAR_ALLOCATOR_PUSH(&local, bytes, 4);

    assert_test(bytes.begin == (uint8_t *)data.begin);
    assert_test(SLICE_BYTESIZE(bytes) == 4);
    assert_test(local.cursor == bytes.end);

    LINEAR_ALLOCATOR_POP(&local, bytes);

    linear_allocator_pop(allocator, data);
}

#ifdef APP_ASSERTIONS
PRIVATE void test_linear_allocator_push_panics_on_overflow(linear_allocator_t *allocator) {
    slice_t data = linear_allocator_push(allocator, 4);
    linear_allocator_t local = linear_allocator_init(data);

    expect_panic_begin();
    linear_allocator_push(&local, 8);
    assert_test(expect_panic_end());

    linear_allocator_pop(allocator, data);
}
#endif

PRIVATE void test_linear_allocator_push_alignment(linear_allocator_t *allocator) {
    slice_t outer_align = linear_allocator_push_alignment(allocator, 8);
    slice_t data = linear_allocator_push(allocator, 16);
    linear_allocator_t local = linear_allocator_init(data);

    slice_uint8_t byte = LINEAR_ALLOCATOR_PUSH(&local, byte, 1);

    slice_uint32_t witness;
    slice_t padding = LINEAR_ALLOCATOR_PUSH_ALIGNMENT(&local, witness);
    assert_test(SLICE_BYTESIZE(padding) == _Alignof(uint32_t) - 1);

    slice_uint32_t value = LINEAR_ALLOCATOR_PUSH(&local, value, 1);
    assert_test((uintptr_t)value.begin % _Alignof(uint32_t) == 0);

    LINEAR_ALLOCATOR_POP(&local, value);
    linear_allocator_pop(&local, padding);
    LINEAR_ALLOCATOR_POP(&local, byte);

    linear_allocator_pop(allocator, data);
    linear_allocator_pop(allocator, outer_align);
}

#ifdef APP_ASSERTIONS
PRIVATE void test_linear_allocator_push_alignment_panics_on_non_power_of_two_alignment(linear_allocator_t *allocator) {
    slice_t data = linear_allocator_push(allocator, 16);
    linear_allocator_t local = linear_allocator_init(data);

    expect_panic_begin();
    linear_allocator_push_alignment(&local, 3);
    assert_test(expect_panic_end());

    linear_allocator_pop(allocator, data);
}
#endif

PRIVATE void test_linear_allocator_pop(linear_allocator_t *allocator) {
    slice_t data = linear_allocator_push(allocator, 16);
    linear_allocator_t local = linear_allocator_init(data);

    slice_uint8_t bytes = LINEAR_ALLOCATOR_PUSH(&local, bytes, 8);

    LINEAR_ALLOCATOR_POP(&local, bytes);

    assert_test(local.cursor == local.data.begin);

    linear_allocator_pop(allocator, data);
}

#ifdef APP_ASSERTIONS
PRIVATE void test_linear_allocator_pop_panics_on_marker_before_data_begin(linear_allocator_t *allocator) {
    slice_t data = linear_allocator_push(allocator, 16);
    linear_allocator_t local = linear_allocator_init(data);

    slice_t marker = { byteoffset(data.begin, -1), local.cursor };

    expect_panic_begin();
    linear_allocator_pop(&local, marker);
    assert_test(expect_panic_end());

    linear_allocator_pop(allocator, data);
}

PRIVATE void test_linear_allocator_pop_panics_on_marker_end_mismatch(linear_allocator_t *allocator) {
    slice_t data = linear_allocator_push(allocator, 16);
    linear_allocator_t local = linear_allocator_init(data);

    slice_uint8_t bytes = LINEAR_ALLOCATOR_PUSH(&local, bytes, 8);
    slice_t marker = { bytes.begin, byteoffset(bytes.end, -1) };

    expect_panic_begin();
    linear_allocator_pop(&local, marker);
    assert_test(expect_panic_end());

    linear_allocator_pop(allocator, data);
}
#endif

PRIVATE void test_linear_allocator_pop_move(linear_allocator_t *allocator) {
    slice_t data = linear_allocator_push(allocator, 64);
    linear_allocator_t local = linear_allocator_init(data);

    slice_uint8_t a = LINEAR_ALLOCATOR_PUSH(&local, a, 4);
    SLICE_AT(a, 0) = 'A'; SLICE_AT(a, 1) = 'A';
    SLICE_AT(a, 2) = 'A'; SLICE_AT(a, 3) = 'A';

    slice_uint8_t b = LINEAR_ALLOCATOR_PUSH(&local, b, 4);

    slice_uint8_t c = LINEAR_ALLOCATOR_PUSH(&local, c, 4);
    SLICE_AT(c, 0) = 'C'; SLICE_AT(c, 1) = 'C';
    SLICE_AT(c, 2) = 'C'; SLICE_AT(c, 3) = 'C';

    LINEAR_ALLOCATOR_POP_MOVE(&local, c, b);

    assert_test(SLICE_AT(b, 0) == 'C');
    assert_test(SLICE_AT(b, 1) == 'C');
    assert_test(SLICE_AT(b, 2) == 'C');
    assert_test(SLICE_AT(b, 3) == 'C');
    assert_test(local.cursor == byteoffset(b.begin, 4));

    linear_allocator_pop(&local, (slice_t){ b.begin, local.cursor });
    linear_allocator_pop(&local, a.slice);

    linear_allocator_pop(allocator, data);
}

#ifdef APP_ASSERTIONS
PRIVATE void test_linear_allocator_pop_move_panics_on_move_forward(linear_allocator_t *allocator) {
    slice_t data = linear_allocator_push(allocator, 64);
    linear_allocator_t local = linear_allocator_init(data);

    slice_uint8_t a = LINEAR_ALLOCATOR_PUSH(&local, a, 4);
    slice_uint8_t b = LINEAR_ALLOCATOR_PUSH(&local, b, 4);
    (void)a;

    slice_t to = { byteoffset(b.begin, 4), byteoffset(b.begin, 8) };

    expect_panic_begin();
    linear_allocator_pop_move(&local, b.slice, to);
    assert_test(expect_panic_end());

    linear_allocator_pop(allocator, data);
}

PRIVATE void test_linear_allocator_pop_move_panics_on_from_not_top_of_stack(linear_allocator_t *allocator) {
    slice_t data = linear_allocator_push(allocator, 64);
    linear_allocator_t local = linear_allocator_init(data);

    slice_uint8_t a = LINEAR_ALLOCATOR_PUSH(&local, a, 4);
    slice_uint8_t b = LINEAR_ALLOCATOR_PUSH(&local, b, 4);
    (void)b;

    slice_t to = { a.begin, a.begin };

    expect_panic_begin();
    linear_allocator_pop_move(&local, a.slice, to);
    assert_test(expect_panic_end());

    linear_allocator_pop(allocator, data);
}
#endif

PRIVATE void test_linear_allocator_insert(linear_allocator_t *allocator) {
    slice_t data = linear_allocator_push(allocator, 64);
    linear_allocator_t local = linear_allocator_init(data);

    slice_uint8_t a = LINEAR_ALLOCATOR_PUSH(&local, a, 4);
    SLICE_AT(a, 0) = 'A'; SLICE_AT(a, 1) = 'A';
    SLICE_AT(a, 2) = 'A'; SLICE_AT(a, 3) = 'A';

    slice_uint8_t b = LINEAR_ALLOCATOR_PUSH(&local, b, 4);
    SLICE_AT(b, 0) = 'B'; SLICE_AT(b, 1) = 'B';
    SLICE_AT(b, 2) = 'B'; SLICE_AT(b, 3) = 'B';

    void *old_cursor = local.cursor;

    linear_allocator_insert(&local, a.end, 4);

    assert_test(local.cursor == byteoffset(old_cursor, 4));
    assert_test(SLICE_AT(a, 0) == 'A');

    uint8_t *shifted_b = byteoffset(a.end, 4);
    assert_test(shifted_b[0] == 'B'); assert_test(shifted_b[1] == 'B');
    assert_test(shifted_b[2] == 'B'); assert_test(shifted_b[3] == 'B');

    linear_allocator_pop(&local, (slice_t){ a.end, local.cursor });
    linear_allocator_pop(&local, a.slice);

    linear_allocator_pop(allocator, data);
}

#ifdef APP_ASSERTIONS
PRIVATE void test_linear_allocator_insert_panics_on_position_before_data_begin(linear_allocator_t *allocator) {
    slice_t data = linear_allocator_push(allocator, 16);
    linear_allocator_t local = linear_allocator_init(data);

    void *before = byteoffset(data.begin, -1);

    expect_panic_begin();
    linear_allocator_insert(&local, before, 4);
    assert_test(expect_panic_end());

    linear_allocator_pop(allocator, data);
}

PRIVATE void test_linear_allocator_insert_panics_on_position_after_cursor(linear_allocator_t *allocator) {
    slice_t data = linear_allocator_push(allocator, 16);
    linear_allocator_t local = linear_allocator_init(data);

    void *after = byteoffset(local.cursor, 1);

    expect_panic_begin();
    linear_allocator_insert(&local, after, 4);
    assert_test(expect_panic_end());

    linear_allocator_pop(allocator, data);
}
#endif

PRIVATE void test_linear_allocator_copy(linear_allocator_t *allocator) {
    slice_t data = linear_allocator_push(allocator, 16);
    linear_allocator_t local = linear_allocator_init(data);

    slice_uint8_t from = LINEAR_ALLOCATOR_PUSH(&local, from, 4);
    SLICE_AT(from, 0) = 'A'; SLICE_AT(from, 1) = 'B';
    SLICE_AT(from, 2) = 'C'; SLICE_AT(from, 3) = 'D';

    slice_uint8_t to = LINEAR_ALLOCATOR_PUSH(&local, to, 4);

    linear_allocator_copy(&local, from.slice, to.slice);

    assert_test(SLICE_AT(to, 0) == 'A'); assert_test(SLICE_AT(to, 1) == 'B');
    assert_test(SLICE_AT(to, 2) == 'C'); assert_test(SLICE_AT(to, 3) == 'D');

    LINEAR_ALLOCATOR_POP(&local, to);
    LINEAR_ALLOCATOR_POP(&local, from);

    linear_allocator_pop(allocator, data);
}

#ifdef APP_ASSERTIONS
PRIVATE void test_linear_allocator_copy_panics_on_to_begin_before_data_begin(linear_allocator_t *allocator) {
    slice_t data = linear_allocator_push(allocator, 16);
    linear_allocator_t local = linear_allocator_init(data);

    slice_uint8_t from = LINEAR_ALLOCATOR_PUSH(&local, from, 4);
    slice_t to = { byteoffset(local.data.begin, -1), byteoffset(local.data.begin, 3) };

    expect_panic_begin();
    linear_allocator_copy(&local, from.slice, to);
    assert_test(expect_panic_end());

    LINEAR_ALLOCATOR_POP(&local, from);
    linear_allocator_pop(allocator, data);
}

PRIVATE void test_linear_allocator_copy_panics_on_to_end_after_data_end(linear_allocator_t *allocator) {
    slice_t data = linear_allocator_push(allocator, 16);
    linear_allocator_t local = linear_allocator_init(data);

    slice_uint8_t from = LINEAR_ALLOCATOR_PUSH(&local, from, 4);
    slice_t to = { byteoffset(local.data.end, -3), byteoffset(local.data.end, 1) };

    expect_panic_begin();
    linear_allocator_copy(&local, from.slice, to);
    assert_test(expect_panic_end());

    LINEAR_ALLOCATOR_POP(&local, from);
    linear_allocator_pop(allocator, data);
}
#endif

PRIVATE void test_slice_at(linear_allocator_t *allocator) {
    slice_uint8_t s = LINEAR_ALLOCATOR_PUSH(allocator, s, 4);
    SLICE_AT(s, 0) = 1; SLICE_AT(s, 1) = 2; SLICE_AT(s, 2) = 3; SLICE_AT(s, 3) = 4;

    assert_test(SLICE_AT(s, 0) == 1);
    assert_test(SLICE_AT(s, 2) == 3);

    SLICE_AT(s, 1) = 42;
    assert_test(SLICE_AT(s, 1) == 42);

    LINEAR_ALLOCATOR_POP(allocator, s);
}

#ifdef APP_ASSERTIONS
PRIVATE void test_slice_at_panics_on_non_power_of_two_alignment(linear_allocator_t *allocator) {
    slice_t s = linear_allocator_push(allocator, 8);

    expect_panic_begin();
    slice_at(s, 0, 3);
    assert_test(expect_panic_end());

    linear_allocator_pop(allocator, s);
}

PRIVATE void test_slice_at_panics_on_out_of_bounds(linear_allocator_t *allocator) {
    slice_t s = linear_allocator_push(allocator, 4);

    expect_panic_begin();
    slice_at(s, 8, 1);
    assert_test(expect_panic_end());

    linear_allocator_pop(allocator, s);
}

PRIVATE void slice_at_panics_on_index_equal_to_count(linear_allocator_t* allocator) {
    slice_t s = linear_allocator_push(allocator, 4);

    expect_panic_begin();
    slice_at(s, 4, 1);
    assert_test(expect_panic_end());

    linear_allocator_pop(allocator, s);
}

PRIVATE void test_slice_at_panics_on_misalignment(linear_allocator_t *allocator) {
    slice_t outer_align = linear_allocator_push_alignment(allocator, 4);
    slice_t s = linear_allocator_push(allocator, 8);

    expect_panic_begin();
    slice_at(s, 1, 4);
    assert_test(expect_panic_end());

    linear_allocator_pop(allocator, s);
    linear_allocator_pop(allocator, outer_align);
}
#endif

PRIVATE void test_slice_advance(linear_allocator_t *allocator) {
    slice_uint8_t s = LINEAR_ALLOCATOR_PUSH(allocator, s, 4);
    SLICE_AT(s, 0) = 1; SLICE_AT(s, 1) = 2; SLICE_AT(s, 2) = 3; SLICE_AT(s, 3) = 4;

    slice_uint8_t advanced = SLICE_ADVANCE(s, 2);

    assert_test(advanced.begin == s.begin + 2);
    assert_test(advanced.end == s.end);
    assert_test(SLICE_AT(advanced, 0) == 3);

    LINEAR_ALLOCATOR_POP(allocator, s);
}

#ifdef APP_ASSERTIONS
PRIVATE void test_slice_advance_panics_on_out_of_bounds(linear_allocator_t *allocator) {
    slice_uint8_t s = LINEAR_ALLOCATOR_PUSH(allocator, s, 4);

    expect_panic_begin();
    (void)SLICE_ADVANCE(s, 8);
    assert_test(expect_panic_end());

    LINEAR_ALLOCATOR_POP(allocator, s);
}
#endif

PRIVATE void test_bytesize(linear_allocator_t *allocator) {
    slice_uint8_t s = LINEAR_ALLOCATOR_PUSH(allocator, s, 7);

    assert_test(SLICE_BYTESIZE(s) == 7);

    LINEAR_ALLOCATOR_POP(allocator, s);
}

const test_case_t g_memory_tests[] = {
    { TEST_NAME("byteoffset"), test_byteoffset },
    { TEST_NAME("linear_allocator_init"), test_linear_allocator_init },
    { TEST_NAME("linear_allocator_deinit"), test_linear_allocator_deinit },
#ifdef APP_ASSERTIONS
    { TEST_NAME("linear_allocator_deinit_panics_on_leftover_allocation"), test_linear_allocator_deinit_panics_on_leftover_allocation },
#endif
    { TEST_NAME("linear_allocator_push"), test_linear_allocator_push },
#ifdef APP_ASSERTIONS
    { TEST_NAME("linear_allocator_push_panics_on_overflow"), test_linear_allocator_push_panics_on_overflow },
#endif
    { TEST_NAME("linear_allocator_push_alignment"), test_linear_allocator_push_alignment },
#ifdef APP_ASSERTIONS
    { TEST_NAME("linear_allocator_push_alignment_panics_on_non_power_of_two_alignment"), test_linear_allocator_push_alignment_panics_on_non_power_of_two_alignment },
#endif
    { TEST_NAME("linear_allocator_pop"), test_linear_allocator_pop },
#ifdef APP_ASSERTIONS
    { TEST_NAME("linear_allocator_pop_panics_on_marker_before_data_begin"), test_linear_allocator_pop_panics_on_marker_before_data_begin },
    { TEST_NAME("linear_allocator_pop_panics_on_marker_end_mismatch"), test_linear_allocator_pop_panics_on_marker_end_mismatch },
#endif
    { TEST_NAME("linear_allocator_pop_move"), test_linear_allocator_pop_move },
#ifdef APP_ASSERTIONS
    { TEST_NAME("linear_allocator_pop_move_panics_on_move_forward"), test_linear_allocator_pop_move_panics_on_move_forward },
    { TEST_NAME("linear_allocator_pop_move_panics_on_from_not_top_of_stack"), test_linear_allocator_pop_move_panics_on_from_not_top_of_stack },
#endif
    { TEST_NAME("linear_allocator_insert"), test_linear_allocator_insert },
#ifdef APP_ASSERTIONS
    { TEST_NAME("linear_allocator_insert_panics_on_position_before_data_begin"), test_linear_allocator_insert_panics_on_position_before_data_begin },
    { TEST_NAME("linear_allocator_insert_panics_on_position_after_cursor"), test_linear_allocator_insert_panics_on_position_after_cursor },
#endif
    { TEST_NAME("linear_allocator_copy"), test_linear_allocator_copy },
#ifdef APP_ASSERTIONS
    { TEST_NAME("linear_allocator_copy_panics_on_to_begin_before_data_begin"), test_linear_allocator_copy_panics_on_to_begin_before_data_begin },
    { TEST_NAME("linear_allocator_copy_panics_on_to_end_after_data_end"), test_linear_allocator_copy_panics_on_to_end_after_data_end },
#endif
    { TEST_NAME("slice_at"), test_slice_at },
#ifdef APP_ASSERTIONS
    { TEST_NAME("slice_at_panics_on_non_power_of_two_alignment"), test_slice_at_panics_on_non_power_of_two_alignment },
    { TEST_NAME("slice_at_panics_on_out_of_bounds"), test_slice_at_panics_on_out_of_bounds },
    { TEST_NAME("slice_at_panics_on_index_equal_to_count"), slice_at_panics_on_index_equal_to_count },
    { TEST_NAME("slice_at_panics_on_misalignment"), test_slice_at_panics_on_misalignment },
#endif
    { TEST_NAME("slice_advance"), test_slice_advance },
#ifdef APP_ASSERTIONS
    { TEST_NAME("slice_advance_panics_on_out_of_bounds"), test_slice_advance_panics_on_out_of_bounds },
#endif
    { TEST_NAME("bytesize"), test_bytesize },
};

const uint32_t g_memory_tests_count = sizeof(g_memory_tests) / sizeof(g_memory_tests[0]);
