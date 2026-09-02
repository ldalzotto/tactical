#ifdef APP_COVERAGE

// Compiled only when APP_COVERAGE is on. clang's -fprofile-instr-generate
// normally links compiler-rt's profile runtime (not installed here); these
// symbols exist only to satisfy the linker. Counters are read directly out
// of wasm memory by server/wasm-profile.js, so __llvm_profile_write_file
// is never needed.
int __llvm_profile_runtime = 0;

// wasm32 is freestanding, so clang can't use the ELF/COFF linker-section
// trick to discover instrumented functions -- it emits an explicit call to
// this pair per function instead, normally consumed by compiler-rt's
// registration constructor. Nothing reads that list here (see above), so
// both are no-ops; only the symbol names need to resolve.
// no_profile_instrument_function stops clang instrumenting these two
// themselves -- their constructor never fires in this freestanding build,
// so their own counters would sit at 0 forever and coverage.js would flag
// that as a gap.
__attribute__((no_profile_instrument_function))
void __llvm_profile_register_function(void *data) {
    (void)data;
}

__attribute__((no_profile_instrument_function))
void __llvm_profile_register_names_function(void *names_start, unsigned long long names_size) {
    (void)names_start;
    (void)names_size;
}

#endif