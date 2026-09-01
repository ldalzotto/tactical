#include "test.h"
#include "lib/assert.h"
#include "lib/linkage.h"
#include "lib/memory.h"
#include "lib/runtime.h"
#include <stddef.h>
#include <stdint.h>
#ifdef APP_BUILD_TEST_SUITES
#include "test_runtime.h"
#include "test_app.h"
#include "test_memory.h"
#include "test_fmt.h"
#include "test_layout.h"
#include "test_game_movement.h"
#include "test_game_combat.h"
#include "test_game_aoe.h"
#include "test_game_ai.h"
#include "test_game_selection.h"
#include "test_scenario.h"
#include "test_render.h"
#include "test_debug_print.h"
#endif
#ifdef APP_BUILD_FUZZ_TESTS
#include "test_game_fuzz.h"
#endif

// g_*_tests_count is extern const, not a compile-time constant, so it can't
// seed a static initializer -- dispatch suite-by-suite at runtime instead.

PRIVATE const test_case_t *test_lookup(uint32_t index) {
#ifdef APP_BUILD_TEST_SUITES
    if (index < g_runtime_tests_count) { return &g_runtime_tests[index]; }
    index -= g_runtime_tests_count;

    if (index < g_app_tests_count) { return &g_app_tests[index]; }
    index -= g_app_tests_count;

    if (index < g_memory_tests_count) { return &g_memory_tests[index]; }
    index -= g_memory_tests_count;

    if (index < g_fmt_tests_count) { return &g_fmt_tests[index]; }
    index -= g_fmt_tests_count;

    if (index < g_layout_tests_count) { return &g_layout_tests[index]; }
    index -= g_layout_tests_count;

    if (index < g_game_movement_tests_count) { return &g_game_movement_tests[index]; }
    index -= g_game_movement_tests_count;

    if (index < g_game_combat_tests_count) { return &g_game_combat_tests[index]; }
    index -= g_game_combat_tests_count;

    if (index < g_game_aoe_tests_count) { return &g_game_aoe_tests[index]; }
    index -= g_game_aoe_tests_count;

    if (index < g_game_ai_tests_count) { return &g_game_ai_tests[index]; }
    index -= g_game_ai_tests_count;

    if (index < g_game_selection_tests_count) { return &g_game_selection_tests[index]; }
    index -= g_game_selection_tests_count;

    if (index < g_scenario_tests_count) { return &g_scenario_tests[index]; }
    index -= g_scenario_tests_count;

    if (index < g_render_tests_count) { return &g_render_tests[index]; }
    index -= g_render_tests_count;

    if (index < g_debug_print_tests_count) { return &g_debug_print_tests[index]; }
    index -= g_debug_print_tests_count;
#endif

    // Guarded even though g_game_fuzz_tests_count is always defined (0 when
    // fuzzing is off): unguarded, this branch would be unreachable in
    // non-fuzz builds and coverage would flag it as a gap.
#ifdef APP_BUILD_FUZZ_TESTS
    if (index < g_game_fuzz_tests_count) { return &g_game_fuzz_tests[index]; }
#endif

    assert_test(false);
    // Reached only if assert_test's panic is swallowed (see
    // test_discovery_out_of_range_panics). Must be a real object, not NULL:
    // callers deref the fields unconditionally, and -O3 can trap on NULL UB.
    static const test_case_t out_of_range = { { 0, 0 }, 0 };
    return &out_of_range;
}

__attribute__((export_name("test_discovery_count")))
uint32_t test_discovery_count(void) {
    uint32_t count = 0;
#ifdef APP_BUILD_TEST_SUITES
    count += g_runtime_tests_count
        + g_app_tests_count
        + g_memory_tests_count
        + g_fmt_tests_count
        + g_layout_tests_count
        + g_game_movement_tests_count
        + g_game_combat_tests_count
        + g_game_aoe_tests_count
        + g_game_ai_tests_count
        + g_game_selection_tests_count
        + g_scenario_tests_count
        + g_render_tests_count
        + g_debug_print_tests_count;
#endif
#ifdef APP_BUILD_FUZZ_TESTS
    count += g_game_fuzz_tests_count;
#endif
    return count;
}

__attribute__((export_name("test_discovery_name_begin")))
const char *test_discovery_name_begin(uint32_t index) {
    return (const char *)test_lookup(index)->name.begin;
}

__attribute__((export_name("test_discovery_name_end")))
const char *test_discovery_name_end(uint32_t index) {
    return (const char *)test_lookup(index)->name.end;
}

__attribute__((export_name("test_discovery_fn_at")))
test_fn_t test_discovery_fn_at(uint32_t index) {
    return test_lookup(index)->fn;
}

__attribute__((export_name("test_run")))
void test_run(test_fn_t fn, uint32_t memory_size) {
    expect_panic_end();
    expect_trap_end();

    slice_t data = { heap_base(), byteoffset(heap_base(), (ptrdiff_t)memory_size) };
    linear_allocator_t allocator = linear_allocator_init(data);

    fn(&allocator);

    linear_allocator_deinit(&allocator);
}

__attribute__((export_name("test_expect_trap_end")))
bool test_expect_trap_end(void) {
    return expect_trap_end();
}
