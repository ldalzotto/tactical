#pragma once

__attribute__((import_module("env"), import_name("report_panic")))
extern void report_panic(const char *file, int file_len, int line, const char *msg, int msg_len);

static inline int panic_strlen(const char *s) {
    int len = 0;
    while (s[len]) {
        len++;
    }
    return len;
}

__attribute__((noreturn))
static inline void panic(const char *file, int line, const char *msg) {
    report_panic(file, panic_strlen(file), line, msg, panic_strlen(msg));
    __builtin_trap();
}

#define PANIC(msg) panic(__FILE__, __LINE__, (msg))
#define ASSERT(cond) do { if (!(cond)) { PANIC("assertion failed: " #cond); } } while (0)
