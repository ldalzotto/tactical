// Compiled only when APP_COVERAGE is ON.
//
// clang's -fprofile-instr-generate normally links against compiler-rt's
// profile runtime (libclang_rt.profile-wasm32.a), which is not installed
// here. The instrumented wasm only needs this one data symbol at link time;
// the live counters are read directly out of wasm memory by
// server/wasm-profile.js (and no __llvm_profile_write_file is required).
int __llvm_profile_runtime = 0;
