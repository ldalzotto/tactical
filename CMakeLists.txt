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

set(APP_SOURCES
    src/lib/assert.c
    src/lib/clock.c
    src/lib/graphics.c
    src/lib/memory.c
    src/lib/runtime.c

    src/game/entity.c
    src/game/grid.c
    src/game/input.c
    src/game/layout.c
    src/game/pathing.c
    src/game/turn.c

    src/main.c
)

if(APP_BUILD_TESTS)
    list(APPEND APP_SOURCES src/test.c)
endif()

add_executable(app ${APP_SOURCES})

if(APP_BUILD_TESTS)
    target_compile_definitions(app PRIVATE APP_BUILD_TESTS)
endif()

target_compile_options(app PRIVATE -ffreestanding -fno-builtin)
target_compile_options(app PRIVATE $<$<CONFIG:Debug>:-g -O0>)
target_compile_options(app PRIVATE $<$<CONFIG:Release>:-O3>)
target_compile_definitions(app PRIVATE $<$<CONFIG:Release>:NDEBUG>)

find_program(WASM_LD wasm-ld REQUIRED)
target_link_options(app PRIVATE -nostdlib -fuse-ld=${WASM_LD} -Wl,--no-entry -Wl,--import-memory)

set_target_properties(app PROPERTIES
    SUFFIX ".wasm"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
