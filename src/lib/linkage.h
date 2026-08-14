#pragma once

// PUBLIC marks a function that's part of a module's API: declared in its
// header, defined in its .c, and called from other .c files. Apply it
// identically to both the header prototype and the .c definition -- C
// requires their storage class to match.
//
// Normal (per-file) build: each .c file is its own translation unit, so
// these need real external linkage to link across files -> expands to
// nothing.
//
// Unity build (APP_UNITY_BUILD): every .c file is folded
// into one translation unit, so no symbol outside this binary ever needs
// the name -> expands to `static`, so the compiler can prove each one is
// unused after inlining and fully fold or drop it, the same way it
// already does for PRIVATE helpers.
#ifdef APP_UNITY_BUILD
#define PUBLIC static
#else
#define PUBLIC
#endif

// PRIVATE marks a function only ever used within its own .c file. Always
// static, in every build mode.
//
// Neither macro applies to the app's actual wasm boundary functions
// (__attribute__((export_name("..."))) in src/app.c, src/test.c, etc.) --
// those stay plain extern definitions, since the attribute is what keeps
// them alive through --gc-sections regardless of linkage.
#define PRIVATE static
