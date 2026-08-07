cmake_minimum_required(VERSION 3.20)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR wasm32)

set(CMAKE_C_COMPILER clang-22)
set(CMAKE_C_COMPILER_TARGET wasm32)

# Freestanding wasm32 has no entry point and no libc, so the normal
# CMake compiler-check (which tries to link an executable) fails.
# Link a static library instead to validate the compiler works.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(app C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(app src/main.c src/panic.c)

target_compile_options(app PRIVATE -g -O0 -ffreestanding -fno-builtin)

find_program(WASM_LD wasm-ld REQUIRED)
target_link_options(app PRIVATE -nostdlib -fuse-ld=${WASM_LD} -Wl,--no-entry)

set_target_properties(app PROPERTIES
    SUFFIX ".wasm"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
