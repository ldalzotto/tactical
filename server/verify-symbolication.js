'use strict';

// panic_without_expect_panic_traps (src/test_runtime.c) deterministically
// traps via panic(false), giving runWasmTests a real wasm stack to
// symbolicate every run. Checking its resolved detail here is what catches
// symbolication silently degrading -- e.g. wasm-objdump/llvm-symbolizer
// missing from the toolchain -- instead of it only showing up as garbled
// text in an unrelated test failure.
const TARGET_TEST_NAME = 'panic_without_expect_panic_traps';
// panic(false) is written on this exact line of test_runtime.c, inside the
// function that must show up as the resolved trace's outermost frame.
// symbolicate.js takes that frame's function name from the wasm binary's
// name section (via funcIndex) rather than from llvm-symbolizer's own
// DWARF subprogram lookup, which has been observed to misattribute it to
// an unrelated function even when the file:line it reports is correct.
const EXPECTED_FUNCTION = 'test_panic_without_expect_panic_traps';
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
    if (!detail.includes(EXPECTED_FUNCTION) || !detail.includes(EXPECTED_FILE) || !detail.includes(EXPECTED_LINE)) {
        return `expected the stack trace for '${TARGET_TEST_NAME}' to include ${EXPECTED_FUNCTION} at ${EXPECTED_FILE}${EXPECTED_LINE}, got:\n${detail}`;
    }
    return null;
}

module.exports = { checkSymbolicationDetail, TARGET_TEST_NAME };
