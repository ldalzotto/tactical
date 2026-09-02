#pragma once

// PUBLIC marks a function that's part of a module's API: declared in its
// header, defined in its .c, called from other .c files. Apply identically
// to both prototype and definition -- C requires matching storage class.
//
// Per-file build: each .c is its own translation unit, so these need real
// external linkage -> expands to nothing.
//
// Unity build (APP_UNITY_BUILD): all .c files fold into one translation
// unit, so no symbol outside the binary needs the name -> expands to
// `static`, letting the compiler fold/drop each one after inlining, same
// as PRIVATE helpers.
#ifdef APP_UNITY_BUILD
#define PUBLIC static
#else
#define PUBLIC
#endif

// PRIVATE marks a function only used within its own .c file; always static.
//
// Neither macro applies to wasm boundary functions
// (__attribute__((export_name("..."))) in src/app.c, src/test.c, etc.) --
// those stay plain extern, since the attribute keeps them alive through
// --gc-sections regardless of linkage.
#define PRIVATE static
