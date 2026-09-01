#pragma once

#include "linkage.h"

#include "./memory.h"
#include <stdbool.h>
#include <stdint.h>

// Streams formatted debug output straight through runtime.h's debug_write
// (mid-line) / debug_log (line-ending) bridge -- no buffer of its own.
// Number formatting uses a small stack-local buffer per call that never
// outlives the call. Intended for debug_print.h's pretty-printers, not for
// anything the game's runtime behaviour depends on.

// Converts value's decimal digits into buf (which must have room for at
// least 10 chars) and returns how many chars were written. Pure and
// buffer-only, so it's unit-testable without any I/O.
PUBLIC int fmt_uint_to_chars(uint32_t value, char *buf);

// Same, but for a signed value; buf must have room for at least 11 chars
// (10 digits plus a leading '-').
PUBLIC int fmt_int_to_chars(int32_t value, char *buf);

// Writes a fragment of the current debug line (no line break).
PUBLIC void fmt_write(slice_t str);
PUBLIC void fmt_write_int(int32_t value);
PUBLIC void fmt_write_uint(uint32_t value);
PUBLIC void fmt_write_bool(bool value);

// Ends the current debug line, flushing whatever was streamed via
// fmt_write* since the last fmt_end_line.
PUBLIC void fmt_end_line(void);

#ifdef APP_UNITY_BUILD
#include "fmt.c"
#endif
