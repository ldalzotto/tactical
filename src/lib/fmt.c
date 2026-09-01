#include "./fmt.h"
#include "lib/linkage.h"
#include "lib/memory.h"
#include "lib/runtime.h"

PUBLIC int fmt_uint_to_chars(uint32_t value, char *buf) {
    if (value == 0) {
        buf[0] = '0';
        return 1;
    }

    char reversed[10];
    int count = 0;
    while (value > 0) {
        reversed[count++] = (char)('0' + (value % 10));
        value /= 10;
    }

    for (int i = 0; i < count; i++) {
        buf[i] = reversed[count - 1 - i];
    }
    return count;
}

PUBLIC int fmt_int_to_chars(int32_t value, char *buf) {
    if (value < 0) {
        buf[0] = '-';
        // Widen before negating: -(int32_t)INT32_MIN overflows int32_t.
        uint32_t magnitude = (uint32_t)(-(int64_t)value);
        return 1 + fmt_uint_to_chars(magnitude, buf + 1);
    }
    return fmt_uint_to_chars((uint32_t)value, buf);
}

PUBLIC void fmt_write(slice_t str) {
    debug_write(str);
}

PUBLIC void fmt_write_int(int32_t value) {
    char buf[11];
    int count = fmt_int_to_chars(value, buf);
    fmt_write((slice_t){ .begin = buf, .end = buf + count });
}

PUBLIC void fmt_write_uint(uint32_t value) {
    char buf[10];
    int count = fmt_uint_to_chars(value, buf);
    fmt_write((slice_t){ .begin = buf, .end = buf + count });
}

PUBLIC void fmt_write_bool(bool value) {
    fmt_write(value ? STR("true") : STR("false"));
}

PUBLIC void fmt_end_line(void) {
    debug_flush_line();
}
