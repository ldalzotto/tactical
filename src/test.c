#include "test.h"
#include "assert.h"

#define TEST_NAME(str) (slice_t){ .begin = (void *)(str), .end = (void *)((str) + sizeof(str) - 1) }

static void test_pass_example(void) {
    assert(1 + 1 == 2);
}

static void test_fail_example(void) {
    assert(1 + 1 == 3);
}

static void test_linear_allocator_pop_move(void) {
    static char buffer[64];
    slice_t data = { buffer, buffer + sizeof(buffer) };
    linear_allocator_t allocator = linear_allocator_init(data);

    slice_t a = linear_allocator_push(&allocator, 4);
    ((char *)a.begin)[0] = 'A'; ((char *)a.begin)[1] = 'A';
    ((char *)a.begin)[2] = 'A'; ((char *)a.begin)[3] = 'A';

    slice_t b = linear_allocator_push(&allocator, 4);
    (void)b;

    slice_t c = linear_allocator_push(&allocator, 4);
    ((char *)c.begin)[0] = 'C'; ((char *)c.begin)[1] = 'C';
    ((char *)c.begin)[2] = 'C'; ((char *)c.begin)[3] = 'C';

    linear_allocator_pop_move(&allocator, c, b);

    assert(((char *)b.begin)[0] == 'C');
    assert(((char *)b.begin)[1] == 'C');
    assert(((char *)b.begin)[2] == 'C');
    assert(((char *)b.begin)[3] == 'C');
    assert(allocator.cursor == byteoffset(b.begin, 4));

    linear_allocator_pop(&allocator, (slice_t){ b.begin, allocator.cursor });
    linear_allocator_pop(&allocator, a);
}

static const test_case_t g_tests[] = {
    { TEST_NAME("pass_example"), test_pass_example },
    { TEST_NAME("fail_example"), test_fail_example },
    { TEST_NAME("linear_allocator_pop_move"), test_linear_allocator_pop_move },
};

#define TEST_COUNT (sizeof(g_tests) / sizeof(g_tests[0]))

__attribute__((export_name("test_discovery_count")))
uint32_t test_discovery_count(void) {
    return TEST_COUNT;
}

__attribute__((export_name("test_discovery_name_begin")))
const char *test_discovery_name_begin(uint32_t index) {
    assert(index < TEST_COUNT);
    return (const char *)g_tests[index].name.begin;
}

__attribute__((export_name("test_discovery_name_end")))
const char *test_discovery_name_end(uint32_t index) {
    assert(index < TEST_COUNT);
    return (const char *)g_tests[index].name.end;
}

__attribute__((export_name("test_discovery_fn_at")))
test_fn_t test_discovery_fn_at(uint32_t index) {
    assert(index < TEST_COUNT);
    return g_tests[index].fn;
}

__attribute__((export_name("test_run")))
void test_run(test_fn_t fn) {
    fn();
}
