#include "test.h"
#include "lib/assert.h"
#include "lib/runtime.h"
#include "test_runtime.h"
#include "test_app.h"
#include "test_memory.h"
#include "test_layout.h"
#include "test_game_movement.h"
#include "test_game_combat.h"
#include "test_game_ai.h"
#include "test_game_selection.h"
#include "test_scenario.h"
#include "test_render.h"
#include "test_game_fuzz.h"

// g_*_tests_count is an extern const, not a compile-time constant in C, so
// it can't seed a static initializer -- these helpers do the same
// suite-by-suite dispatch at runtime instead of building a lookup table.

PRIVATE const test_case_t *test_lookup(uint32_t index) {
    if (index < g_runtime_tests_count) { return &g_runtime_tests[index]; }
    index -= g_runtime_tests_count;

    if (index < g_app_tests_count) { return &g_app_tests[index]; }
    index -= g_app_tests_count;

    if (index < g_memory_tests_count) { return &g_memory_tests[index]; }
    index -= g_memory_tests_count;

    if (index < g_layout_tests_count) { return &g_layout_tests[index]; }
    index -= g_layout_tests_count;

    if (index < g_game_movement_tests_count) { return &g_game_movement_tests[index]; }
    index -= g_game_movement_tests_count;

    if (index < g_game_combat_tests_count) { return &g_game_combat_tests[index]; }
    index -= g_game_combat_tests_count;

    if (index < g_game_ai_tests_count) { return &g_game_ai_tests[index]; }
    index -= g_game_ai_tests_count;

    if (index < g_game_selection_tests_count) { return &g_game_selection_tests[index]; }
    index -= g_game_selection_tests_count;

    if (index < g_scenario_tests_count) { return &g_scenario_tests[index]; }
    index -= g_scenario_tests_count;

    if (index < g_render_tests_count) { return &g_render_tests[index]; }
    index -= g_render_tests_count;

    if (index < g_game_fuzz_tests_count) { return &g_game_fuzz_tests[index]; }

    assert_test(false);
    // Reached only when assert_test's panic is swallowed by expect_panic
    // (see test_discovery_out_of_range_panics). Callers dereference the
    // returned pointer's fields unconditionally, so this must be a real
    // object rather than NULL -- a NULL return, dereferenced right back at
    // the call site, is UB that -O3 is free to turn into a trap.
    static const test_case_t out_of_range = { { 0, 0 }, 0 };
    return &out_of_range;
}

__attribute__((export_name("test_discovery_count")))
uint32_t test_discovery_count(void) {
    return g_runtime_tests_count
        + g_app_tests_count
        + g_memory_tests_count
        + g_layout_tests_count
        + g_game_movement_tests_count
        + g_game_combat_tests_count
        + g_game_ai_tests_count
        + g_game_selection_tests_count
        + g_scenario_tests_count
        + g_render_tests_count
        + g_game_fuzz_tests_count;
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
