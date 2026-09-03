'use strict';

// panic_without_expect_panic_traps (src/test_runtime.c) deterministically
// traps via panic(false), giving runWasmTests a real wasm stack to
// symbolicate every run. Checking its resolved detail here is what catches
// symbolication silently degrading -- e.g. wasm-objdump/llvm-symbolizer
// missing from the toolchain -- instead of it only showing up as garbled
// text in an unrelated test failure.
const TARGET_TEST_NAME = 'panic_without_expect_panic_traps';
// panic(false) is written on this exact line of test_runtime.c; the
// resolved trace's file:line comes from DWARF's .debug_line table and has
// been reliably accurate in testing. The resolved *function name* for that
// frame, by contrast, comes from DWARF subprogram (.debug_info) lookup and
// has been observed to be wrong in this toolchain (e.g. reporting a
// different static test function than the one that actually panicked) even
// when the file:line is correct -- so it isn't checked here.
const EXPECTED_FILE = 'test_runtime.c';
const EXPECTED_LINE = ':45:';

// Returns an error string if the named test's resolved stack trace doesn't
// look right, or null if there's nothing to check (wrong test, or nothing
// went wrong).
function checkSymbolicationDetail(name, detail) {
    if (name !== TARGET_TEST_NAME) {
        return null;
    }
    if (!detail || detail.includes('symbolication failed')) {
        return `expected a resolved stack trace for '${TARGET_TEST_NAME}', got: ${detail}`;
    }
    if (!detail.includes(EXPECTED_FILE) || !detail.includes(EXPECTED_LINE)) {
        return `expected the stack trace for '${TARGET_TEST_NAME}' to include a frame at ${EXPECTED_FILE}${EXPECTED_LINE}, got:\n${detail}`;
    }
    return null;
}

module.exports = { checkSymbolicationDetail, TARGET_TEST_NAME };
