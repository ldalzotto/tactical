#ifdef APP_COVERAGE

// Compiled only when APP_COVERAGE is ON.
//
// clang's -fprofile-instr-generate normally links against compiler-rt's
// profile runtime (libclang_rt.profile-wasm32.a), which is not installed
// here. The instrumented wasm only needs the symbols below at link time;
// the live counters are read directly out of wasm memory by
// server/wasm-profile.js (and no __llvm_profile_write_file is required).
int __llvm_profile_runtime = 0;

// wasm32 is freestanding, so clang can't rely on the ELF/COFF linker-section
// trick to discover instrumented functions at startup -- it instead emits an
// explicit call to this pair per function, normally run by compiler-rt's
// runtime constructor to build its registration list. Nothing here ever
// reads that list (see above), so both are no-ops; only the symbol names
// need to resolve. no_profile_instrument_function keeps clang from
// instrumenting these two themselves -- otherwise their own counters sit at
// 0 forever (the registration constructor that would run them never fires
// in this freestanding build) and coverage.js flags that as a gap.
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