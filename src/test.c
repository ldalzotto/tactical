#include "test.h"
#include "assert.h"

#define TEST_NAME(str) (slice_t){ .begin = (void *)(str), .end = (void *)((str) + sizeof(str) - 1) }

static void test_pass_example(void) {
    assert(1 + 1 == 2);
}

static void test_fail_example(void) {
    assert(1 + 1 == 3);
}

static const test_case_t g_tests[] = {
    { TEST_NAME("pass_example"), test_pass_example },
    { TEST_NAME("fail_example"), test_fail_example },
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
