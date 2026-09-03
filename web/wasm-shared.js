function parseTrapFrames(stack) {
    const re = /wasm-function\[(\d+)\]:(0x[0-9a-fA-F]+)/g;
    const frames = [];
    let match;
    while ((match = re.exec(stack)) !== null) {
        frames.push({ funcIndex: Number(match[1]), offset: match[2] });
    }
    return frames;
}

function formatResolvedFrame(frame) {
    if (!frame.locations || frame.locations.length === 0) {
        return `  wasm-function[${frame.funcIndex}]:${frame.offset} (unresolved)`;
    }
    return frame.locations
        .map((loc) => `  ${loc.function} at ${loc.file}:${loc.line}:${loc.column}`)
        .join('\n');
}

async function resolveFailureText(err, resolveFrames) {
    const message = err.message || String(err);
    const rawFrames = parseTrapFrames(err.stack || '');
    if (rawFrames.length === 0) {
        return { message, framesText: '' };
    }
    try {
        const resolvedFrames = await resolveFrames(rawFrames);
        return { message, framesText: resolvedFrames.map(formatResolvedFrame).join('\n') };
    } catch (symbolicateErr) {
        return { message, framesText: `(symbolication failed: ${symbolicateErr.message})` };
    }
}

function decodeWasmMemoryString(memory, ptr, len) {
    const bytes = new Uint8Array(memory.buffer, ptr, len);
    return new TextDecoder().decode(bytes);
}

// Wasm memory is imported (not module-owned), so JS sets the budget: 32
// pages (2 MiB) covers static data/stack/framebuffer with headroom; bump if
// the app grows. Too small traps in the C-side allocator, not silent corruption.
const MEMORY_PAGES = 32;

const INPUT_EVENT_BYTE_SIZE = 12;

function buildImportObject({ createWindow, presentWindow, printLine, reportPanic }) {
    const memory = new WebAssembly.Memory({ initial: MEMORY_PAGES });
    const pendingInputEvents = new Map();
    // Fragments streamed via write accumulate here until the next
    // flush_line call closes the line -- lets wasm build one line
    // out of many small writes (see fmt.h) without a C-side buffer.
    let pendingLine = '';

    function pushInputEvent(windowHandle, type, x, y) {
        let events = pendingInputEvents.get(windowHandle);
        if (!events) {
            events = [];
            pendingInputEvents.set(windowHandle, events);
        }
        events.push({ type, x, y });
    }

    const importObject = {
        env: {
            memory,
            create_window: createWindow ?? (() => 0),
            present_window(windowHandle, fbBegin, fbEnd) {
                (presentWindow ?? (() => {}))(windowHandle, new Uint8ClampedArray(memory.buffer, fbBegin, fbEnd - fbBegin));
            },
            write(beginPtr, endPtr) {
                pendingLine += decodeWasmMemoryString(memory, beginPtr, endPtr - beginPtr);
            },
            flush_line() {
                printLine(pendingLine);
                pendingLine = '';
            },
            report_panic(fileBegin, fileEnd, line, msgBegin, msgEnd) {
                const file = decodeWasmMemoryString(memory, fileBegin, fileEnd - fileBegin);
                const message = decodeWasmMemoryString(memory, msgBegin, msgEnd - msgBegin);
                if (reportPanic) {
                    reportPanic({ file, line, message });
                }
                throw new Error(`panic: ${file}:${line}: ${message}`);
            },
            // Test-only hook: lets C tests queue an input event so
            // app_on_next_frame's dispatch path gets a non-empty batch (the
            // wasm test runner otherwise always polls zero events).
            test_push_input_event(windowHandle, type, x, y) {
                pushInputEvent(windowHandle, type, x, y);
            },
            poll_input_events(windowHandle, beginPtr) {
                const events = pendingInputEvents.get(windowHandle) ?? [];
                const writeCount = events.length;
                const view = new DataView(memory.buffer);

                for (let i = 0; i < writeCount; i++) {
                    const offset = beginPtr + i * INPUT_EVENT_BYTE_SIZE;
                    view.setInt32(offset, events[i].type, true);
                    view.setInt32(offset + 4, events[i].x, true);
                    view.setInt32(offset + 8, events[i].y, true);
                }

                pendingInputEvents.set(windowHandle, events.slice(writeCount));

                return beginPtr + writeCount * INPUT_EVENT_BYTE_SIZE;
            },
        },
    };

    return { memory, importObject, pushInputEvent };
}

// Called once per test as runWasmTests goes. Returns an error string if the
// named test's resolved stack trace doesn't look right, or null if there's
// nothing to check (wrong test, or nothing went wrong).
function checkSymbolicationDetail(name, detail) {
    // panic_without_expect_panic_traps (src/test_runtime.c) deterministically
    // traps via panic(false), giving runWasmTests a real wasm stack to
    // symbolicate every run. Checking its resolved detail here is what
    // catches symbolication silently degrading -- e.g. wasm-objdump/
    // llvm-symbolizer missing from the toolchain -- instead of it only
    // showing up as garbled text in an unrelated test failure.
    const TEST_NAME = 'panic_without_expect_panic_traps';
    // panic(false) is written on this exact line of test_runtime.c, inside
    // the function that must show up as the resolved trace's outermost
    // frame. symbolicate.js takes that frame's function name from the wasm
    // binary's name section (via funcIndex) rather than from
    // llvm-symbolizer's own DWARF subprogram lookup, which has been
    // observed to misattribute it to an unrelated function even when the
    // file:line it reports is correct.
    const EXPECTED_FUNCTION = 'test_panic_without_expect_panic_traps';
    const EXPECTED_FILE = 'test_runtime.c';
    const EXPECTED_LINE = ':45:';

    if (name !== TEST_NAME) {
        return null;
    }
    if (!detail || detail.includes('symbolication failed')) {
        return `expected a resolved stack trace for '${TEST_NAME}', got: ${detail}`;
    }
    if (!detail.includes(EXPECTED_FUNCTION) || !detail.includes(EXPECTED_FILE) || !detail.includes(EXPECTED_LINE)) {
        return `expected the stack trace for '${TEST_NAME}' to include ${EXPECTED_FUNCTION} at ${EXPECTED_FILE}${EXPECTED_LINE}, got:\n${detail}`;
    }
    return null;
}

async function runWasmTests({ wasmBytes, resolveFrames, onResult, onComplete, createWindow, presentWindow, printLine }) {
    const { memory, importObject } = buildImportObject({ createWindow, presentWindow, printLine });
    const { instance } = await WebAssembly.instantiate(wasmBytes, importObject);
    const { test_discovery_count, test_discovery_name_begin, test_discovery_name_end, test_discovery_fn_at, test_run, test_expect_trap_end } = instance.exports;
    const count = test_discovery_count();

    let passed = 0;
    let failed = 0;
    let symbolicationError = null;

    for (let i = 0; i < count; i++) {
        const beginPtr = test_discovery_name_begin(i);
        const endPtr = test_discovery_name_end(i);
        const name = decodeWasmMemoryString(memory, beginPtr, endPtr - beginPtr);
        const fn = test_discovery_fn_at(i);

        try {
            test_run(fn, memory.buffer.byteLength);
            passed++;
            symbolicationError ??= checkSymbolicationDetail(name, undefined);
            onResult({ name, passed: true });
        } catch (err) {
            const isExpectedTrap = test_expect_trap_end();
            // Resolved even for an expected trap: this is the only place an
            // expected trap's stack is symbolicated, which is what lets
            // checkSymbolicationDetail verify symbolication itself is working.
            const { message, framesText } = await resolveFailureText(err, resolveFrames);
            const detail = framesText ? `${message}\n${framesText}` : message;
            symbolicationError ??= checkSymbolicationDetail(name, detail);
            if (isExpectedTrap) {
                passed++;
                onResult({ name, passed: true, detail });
                continue;
            }
            failed++;
            onResult({ name, passed: false, detail });
        }
    }

    onComplete({ passed, failed, count, symbolicationError });
    return { passed, failed, count, memory, instance, symbolicationError };
}

async function runApp({ wasmBytes, now, createWindow, presentWindow, printLine, reportPanic }) {
    const { memory, importObject, pushInputEvent } = buildImportObject({
        createWindow: createWindow && ((width, height) => createWindow(width, height, pushInputEvent)),
        presentWindow,
        printLine,
        reportPanic,
    });
    const { instance } = await WebAssembly.instantiate(wasmBytes, importObject);
    const { init, onNextFrame } = instance.exports;

    const statePtr = init(memory.buffer.byteLength, Math.floor(now()));

    function tick() {
        const nowMs = Math.floor(now());
        const waitMs = onNextFrame(statePtr, nowMs);
        setTimeout(tick, waitMs);
    }

    tick();

    return instance;
}

if (typeof module !== 'undefined') {
    module.exports = { parseTrapFrames, formatResolvedFrame, resolveFailureText, decodeWasmMemoryString, runWasmTests, runApp };
}
