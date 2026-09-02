#pragma once

#include "linkage.h"

#include "./memory.h"
#include <stdbool.h>
#include <stdint.h>

// Streams formatted debug output either through runtime.h's debug_write
// (mid-line) / debug_flush_line (line-ending) bridge, or -- when `dest` is
// non-NULL -- onto `dest` itself as a plain growing byte region. Every
// fmt_write* below takes `dest` as its first argument:
//   NULL      -- stream straight to the runtime debug bridge (production
//                use: an agent drops a debug_print_* call site).
//   non-NULL  -- push the formatted bytes onto `dest`. Consecutive pushes
//                on the same allocator land contiguously, so a caller can
//                mark `dest->cursor` before a sequence of fmt_write* calls
//                and read everything written back as one slice afterward
//                (see test_debug_print.c) -- no separate capture/redirect
//                mechanism needed.
// Number formatting still uses a small stack-local buffer per call that
// never outlives the call.

// Converts value's decimal digits into buf (which must have room for at
// least 10 chars) and returns the written prefix of buf. Pure and
// buffer-only, so it's unit-testable without any I/O.
PUBLIC slice_t fmt_uint_to_chars(uint32_t value, slice_t buf);

// Same, but for a signed value; buf must have room for at least 11 chars
// (10 digits plus a leading '-').
PUBLIC slice_t fmt_int_to_chars(int32_t value, slice_t buf);

// Writes a fragment of the current debug line (no line break).
PUBLIC void fmt_write(linear_allocator_t *dest, slice_t str);
PUBLIC void fmt_write_int(linear_allocator_t *dest, int32_t value);
PUBLIC void fmt_write_uint(linear_allocator_t *dest, uint32_t value);
PUBLIC void fmt_write_bool(linear_allocator_t *dest, bool value);

// Ends the current debug line, flushing whatever was streamed via
// fmt_write* since the last fmt_end_line.
PUBLIC void fmt_end_line(linear_allocator_t *dest);

#ifdef APP_UNITY_BUILD
#include "fmt.c"
#endif
