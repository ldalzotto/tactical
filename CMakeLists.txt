cmake_minimum_required(VERSION 3.20)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR wasm32)

set(CMAKE_C_COMPILER clang)
set(CMAKE_C_COMPILER_TARGET wasm32)

# Freestanding wasm32 has no entry point and no libc, so the normal
# CMake compiler-check (which tries to link an executable) fails.
# Link a static library instead to validate the compiler works.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(app C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

message(STATUS "CONFIG: ${CMAKE_BUILD_TYPE}")

option(APP_BUILD_TESTS "Build the non-fuzz test suites into app.wasm" ON)
message(STATUS "APP_BUILD_TESTS: ${APP_BUILD_TESTS}")

option(APP_BUILD_FUZZ_TESTS "Build src/test_game_fuzz.c's seeded fuzz suite into app.wasm" ON)
message(STATUS "APP_BUILD_FUZZ_TESTS: ${APP_BUILD_FUZZ_TESTS}")

option(APP_COVERAGE "Instrument the app with clang source-based code coverage" OFF)
message(STATUS "APP_COVERAGE: ${APP_COVERAGE}")

option(APP_ASSERTIONS "Compile assert_debug checks in (always on for Debug)" OFF)
message(STATUS "APP_ASSERTIONS: ${APP_ASSERTIONS}")

option(APP_DEBUG_SYMBOLS "Emit DWARF debug info for symbolication (always on for Debug)" OFF)
message(STATUS "APP_DEBUG_SYMBOLS: ${APP_DEBUG_SYMBOLS}")

# In unity mode, every lib/*.h and game/*.h (and test*.h, when
# APP_BUILD_TESTS is on) includes only app.c and, conditionally, test.h -- ends up dragging the
# whole app (and test suite) into one translation unit. That, plus
# -ffunction-sections/--gc-sections below, lets the linker drop any
# function that isn't reachable from an export_name root, i.e. treat it
# as if it were static.
option(APP_UNITY_BUILD "Compile the whole app as a single translation unit" OFF)
message(STATUS "APP_UNITY_BUILD: ${APP_UNITY_BUILD}")

if(APP_UNITY_BUILD)
    set(APP_SOURCES src/app.c)
else()
    set(APP_SOURCES
        src/lib/assert.c
        src/lib/clock.c
        src/lib/graphics.c
        src/lib/memory.c
        src/lib/runtime.c

        src/game/action.c
        src/game/ai.c
        src/game/entity.c
        src/game/game.c
        src/game/geometry.c
        src/game/grid.c
        src/game/input.c
        src/game/layout.c
        src/game/pathing.c
        src/game/position.c
        src/game/render.c
        src/game/pathing_ranges.c
        src/game/scenario.c
        src/game/skill.c
        src/game/turn.c
        src/game/ui.c

        src/app.c
    )

    # test.c (the shared discovery/run infra) and test_invariants.c (the
    # assert helpers test_game_helpers.h's click/move wrappers use) are
    # needed by either suite, fuzz or non-fuzz, so they're gated on either
    # flag rather than duplicated per suite.
    if(APP_BUILD_TESTS OR APP_BUILD_FUZZ_TESTS)
        list(APPEND APP_SOURCES
            src/test.c
            src/test_invariants.c
        )
    endif()

    if(APP_BUILD_TESTS)
        list(APPEND APP_SOURCES
            src/test_app.c
            src/test_runtime.c
            src/test_memory.c
            src/test_layout.c
            src/test_game_movement.c
            src/test_game_combat.c
            src/test_game_aoe.c
            src/test_game_ai.c
            src/test_game_selection.c
            src/test_scenario.c
            src/test_render.c
        )
    endif()

    if(APP_BUILD_FUZZ_TESTS)
        list(APPEND APP_SOURCES src/test_game_fuzz.c)
    endif()
endif()

add_executable(app ${APP_SOURCES})

if(APP_COVERAGE)
    target_sources(app PRIVATE src/coverage_stub.c)
endif()

# Test infra (lib/assert.c's expect_panic/expect_trap, lib/memory.c's
# defensive checks, test.c itself) is needed whenever either suite is
# built, so APP_BUILD_TESTS isn't exclusive with APP_BUILD_FUZZ_TESTS: it
# stays defined for a fuzz-only build too. APP_BUILD_TEST_SUITES is the
# narrower signal -- only true when the 8 non-fuzz suite .c files below are
# actually in APP_SOURCES -- for the one place (test.c's suite dispatch)
# that cares about that distinction.
if(APP_BUILD_TESTS OR APP_BUILD_FUZZ_TESTS)
    target_compile_definitions(app PRIVATE APP_BUILD_TESTS)
endif()

if(APP_BUILD_TESTS)
    target_compile_definitions(app PRIVATE APP_BUILD_TEST_SUITES)
endif()

if(APP_BUILD_FUZZ_TESTS)
    target_compile_definitions(app PRIVATE APP_BUILD_FUZZ_TESTS)
endif()

if(APP_UNITY_BUILD)
    target_compile_definitions(app PRIVATE APP_UNITY_BUILD)
endif()

target_compile_options(app PRIVATE -ffreestanding -fno-builtin -mbulk-memory -Wall -Wextra -Werror)
if(APP_COVERAGE)
    target_compile_definitions(app PRIVATE APP_COVERAGE)
    target_compile_options(app PRIVATE -fprofile-instr-generate -fcoverage-mapping)
endif()
target_compile_options(app PRIVATE $<$<CONFIG:Debug>:-O0>)
target_compile_options(app PRIVATE $<$<CONFIG:Release>:-O3>)
target_compile_options(app PRIVATE $<$<OR:$<CONFIG:Debug>,$<BOOL:${APP_DEBUG_SYMBOLS}>>:-g>)
target_compile_definitions(app PRIVATE $<$<OR:$<CONFIG:Debug>,$<BOOL:${APP_ASSERTIONS}>>:APP_ASSERTIONS>)

# Resolve wasm-ld through clang so we always use the linker that matches the
# chosen compiler (find_program would pick up a toolchain proxy shim, e.g.
# swiftly, which breaks with "Circular swiftly proxy invocation").
execute_process(
    COMMAND ${CMAKE_C_COMPILER} --print-prog-name=wasm-ld
    OUTPUT_VARIABLE WASM_LD
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT WASM_LD)
    message(FATAL_ERROR "clang could not locate wasm-ld")
endif()
target_link_options(app PRIVATE -nostdlib -fuse-ld=${WASM_LD} -Wl,--no-entry -Wl,--import-memory)

set_target_properties(app PROPERTIES
    SUFFIX ".wasm"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
