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

option(APP_BUILD_TESTS "Build src/test.c's test-discovery/test-runner exports into app.wasm" ON)
message(STATUS "APP_BUILD_TESTS: ${APP_BUILD_TESTS}")

option(APP_COVERAGE "Instrument the app with clang source-based code coverage" OFF)
message(STATUS "APP_COVERAGE: ${APP_COVERAGE}")

# In unity mode, every lib/*.h and game/*.h (and test*.h, when
# APP_BUILD_TESTS is on) includes only main.c and, conditionally, test.h -- ends up dragging the
# whole app (and test suite) into one translation unit. That, plus
# -ffunction-sections/--gc-sections below, lets the linker drop any
# function that isn't reachable from an export_name root, i.e. treat it
# as if it were static.
option(APP_UNITY_BUILD "Compile the whole app as a single translation unit" OFF)
message(STATUS "APP_UNITY_BUILD: ${APP_UNITY_BUILD}")

if(APP_UNITY_BUILD)
    set(APP_SOURCES src/main.c)
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
        src/game/grid.c
        src/game/input.c
        src/game/layout.c
        src/game/pathing.c
        src/game/position.c
        src/game/render.c
        src/game/render_cache.c
        src/game/scenario.c
        src/game/skill.c
        src/game/turn.c
        src/game/ui.c

        src/main.c
    )

    if(APP_BUILD_TESTS)
        list(APPEND APP_SOURCES
            src/test.c
            src/test_runtime.c
            src/test_memory.c
            src/test_layout.c
            src/test_game_movement.c
            src/test_game_combat.c
            src/test_game_ai.c
            src/test_game_selection.c
            src/test_scenario.c
            src/test_render.c
        )
    endif()
endif()

# Freestanding libc stubs (memcpy/memmove/memset) used by the app.
list(APPEND APP_SOURCES src/lib/libc_stubs.c)

add_executable(app ${APP_SOURCES})

if(APP_COVERAGE)
    target_sources(app PRIVATE src/coverage_stub.c)
endif()

if(APP_BUILD_TESTS)
    target_compile_definitions(app PRIVATE APP_BUILD_TESTS)
endif()

if(APP_UNITY_BUILD)
    target_compile_definitions(app PRIVATE APP_UNITY_BUILD)
endif()

target_compile_options(app PRIVATE -ffreestanding -fno-builtin)
if(APP_COVERAGE)
    target_compile_options(app PRIVATE -fprofile-instr-generate -fcoverage-mapping)
endif()
target_compile_options(app PRIVATE $<$<CONFIG:Debug>:-g -O0>)
target_compile_options(app PRIVATE $<$<CONFIG:Release>:-O3>)
target_compile_definitions(app PRIVATE $<$<CONFIG:Release>:NDEBUG>)

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
