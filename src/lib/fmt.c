#include "./fmt.h"
#include "lib/linkage.h"
#include "lib/memory.h"
#include "lib/runtime.h"

PUBLIC slice_t fmt_uint_to_chars(uint32_t value, slice_t buf) {
    char *begin = buf.begin;
    if (value == 0) {
        begin[0] = '0';
        return (slice_t){ .begin = begin, .end = begin + 1 };
    }

    char reversed[10];
    int count = 0;
    while (value > 0) {
        reversed[count++] = (char)('0' + (value % 10));
        value /= 10;
    }

    for (int i = 0; i < count; i++) {
        begin[i] = reversed[count - 1 - i];
    }
    return (slice_t){ .begin = begin, .end = begin + count };
}

PUBLIC slice_t fmt_int_to_chars(int32_t value, slice_t buf) {
    if (value < 0) {
        char *begin = buf.begin;
        begin[0] = '-';
        // Widen before negating: -(int32_t)INT32_MIN overflows int32_t.
        uint32_t magnitude = (uint32_t)(-(int64_t)value);
        slice_t digits = fmt_uint_to_chars(magnitude, slice_advance(buf, 1));
        return (slice_t){ .begin = begin, .end = digits.end };
    }
    return fmt_uint_to_chars((uint32_t)value, buf);
}

PUBLIC void fmt_write(linear_allocator_t *dest, slice_t str) {
    if (dest != 0) {
        slice_t chunk = linear_allocator_push(dest, (size_t)bytesize(str.begin, str.end));
        linear_allocator_copy(dest, str, chunk);
        return;
    }
    debug_write(str);
}

PUBLIC void fmt_write_int(linear_allocator_t *dest, int32_t value) {
    char buf[11];
    slice_t chars = fmt_int_to_chars(value, (slice_t){ .begin = buf, .end = buf + sizeof(buf) });
    fmt_write(dest, chars);
}

PUBLIC void fmt_write_uint(linear_allocator_t *dest, uint32_t value) {
    char buf[10];
    slice_t chars = fmt_uint_to_chars(value, (slice_t){ .begin = buf, .end = buf + sizeof(buf) });
    fmt_write(dest, chars);
}

PUBLIC void fmt_write_bool(linear_allocator_t *dest, bool value) {
    fmt_write(dest, value ? STR("true") : STR("false"));
}

PUBLIC void fmt_end_line(linear_allocator_t *dest) {
    if (dest != 0) {
        fmt_write(dest, STR("\n"));
        return;
    }
    debug_flush_line();
}
