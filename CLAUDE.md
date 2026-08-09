# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A C application compiled to freestanding wasm32 (no libc, `-ffreestanding -nostdlib`), run in the browser via a small hand-written JS host (`web/wasm-shared.js`). It renders into a `<canvas>` through a manually-managed framebuffer. There's a Node dev server that rebuilds on file change and pushes live reload over SSE, plus a wasm-based test runner (both browser and headless/CI).

## Commands

```bash
npm test                  # build (Debug) then run all wasm tests headlessly via node
```

`npm run build*` / `npm start*` accept `--verbose` to stream cmake/compiler output instead of buffering it.

There is no single-test CLI flag. To run one test, temporarily narrow `g_tests[]` in [src/test.c](src/test.c), or open `http://localhost:8081/tests.html` (via `npm start`) and read results per-test in `#test-panel`.

Build system requires `clang` targeting `wasm32`, `wasm-ld`, and (for stack-trace symbolication) `llvm-symbolizer` and `wasm-objdump` on PATH.

## Architecture

**Build**: `CMakeLists.txt` compiles all of `src/**/*.c` (plus `src/test.c` when `APP_BUILD_TESTS=ON`, the default) into a single freestanding `build/app.wasm`, no entry point, imported (not module-owned) memory. `APP_BUILD_TESTS` gates `expect_panic_begin/end` in [src/lib/assert.c](src/lib/assert.c) and the `test_discovery_*`/`test_run` exports in [src/test.c](src/test.c) — both are compiled out in Release.

**Dev loop** ([server/server.js](server/server.js)): serves `web/` at `/` and `build/` at `/build/`, watches `src/` and `web/` ([server/watcher.js](server/watcher.js)), debounces ([server/debounce.js](server/debounce.js)) and reruns the cmake build ([server/build-runner.js](server/build-runner.js)) on change, then broadcasts a `reload` SSE event ([server/sse-hub.js](server/sse-hub.js)) that `web/dev-reload.js` listens for to refresh the page. `POST /__symbolicate` ([server/symbolicate.js](server/symbolicate.js)) resolves wasm trap addresses to `file:line` using `llvm-symbolizer` against `build/app.wasm`'s DWARF info (offset-corrected via `wasm-objdump -h`) — used both by the browser panic overlay and by `npm test`.

**C ↔ JS boundary**: C exports functions via `__attribute__((export_name(...)))`; JS imports host functions into wasm under module `env` (`memory`, `create_window`, `present_window`, `debug_log`, `report_panic`) — see [src/lib/runtime.c](src/lib/runtime.c) for the C-side extern declarations and [web/wasm-shared.js](web/wasm-shared.js)'s `buildImportObject` for the JS-side implementations. `runApp()` in `wasm-shared.js` instantiates the module, calls `init(memorySize, nowMs)` once, then loop-calls `onNextFrame(statePtr, nowMs)` via `setTimeout`, using its return value as the next wait time (see clock throttling below). `runWasmTests()` drives the same import object but calls the `test_discovery_*` exports to enumerate and `test_run` to invoke each test, catching thrown `report_panic` errors as failures.

**Memory model** ([src/lib/memory.h](src/lib/memory.h)/`.c`): no libc allocator. `heap_base()` (from the linker's `__heap_base` symbol) plus the JS-supplied memory size seeds one `linear_allocator_t` in `init()`. All allocation is stack-discipline push/pop against that single arena — there is no free-list or general-purpose free. `slice_t` (`{begin, end}` pointer pair) is the universal buffer type; `SLICE_DEFINE(T)` generates a typed union wrapper `slice_T` so `.begin`/`.end` are `T*` while `.slice` gives the untyped view. Use the `LINEAR_ALLOCATOR_PUSH`/`_PUSH_ALIGNMENT`/`_POP`/`_POP_MOVE` macros (not the bare `linear_allocator_*` functions) when working with typed slices — they handle the `sizeof`/`_Alignof` math and slice/union conversion. Pops must exactly match the last push (LIFO); mismatches, overflow, and misaligned/out-of-bounds `slice_at` all `assert_debug`-panic (Debug only — compiled out via `NDEBUG` in Release, so don't rely on them for anything security-relevant).

**Panics** ([src/lib/assert.h](src/lib/assert.h)/`.c`): `assert_debug(cond)` (Debug builds only) and `assert_test(cond)` (always, used in tests) both funnel through `panic(bool)`, which `__builtin_trap()`s on failure — surfaced in the browser as a `WebAssembly.RuntimeError`, symbolicated and shown via the panic overlay in `web/main.js`. Inside tests, `expect_panic_begin()`/`expect_panic_end()` (test-only, see [src/test.c](src/test.c) for usage) intercept the trap instead of letting it propagate, so panic-on-invalid-input paths can be asserted against.

**Frame pacing** ([src/lib/clock.h](src/lib/clock.h)/`.c`): `clock_time_to_wait` implements fixed-interval (16 ms / 60 FPS) throttling — `onNextFrame` in [src/main.c](src/main.c) returns early via this if called before the next frame is due; the JS host uses the returned wait as its next `setTimeout` delay.

**Adding a test**: add a `static void test_*(void)` function using `assert_test`/`expect_panic_begin`/`expect_panic_end` in [src/test.c](src/test.c), then register it in the `g_tests[]` array with `TEST_NAME(...)`.
