#pragma once

__attribute__((import_module("env"), import_name("report_panic")))
extern void report_panic(const char *file, int file_len, int line, const char *msg, int msg_len);

int panic_strlen(const char *s);

__attribute__((noreturn))
void panic(const char *file, int line, const char *msg);

#define PANIC(msg) panic(__FILE__, __LINE__, (msg))
#define ASSERT(cond) do { if (!(cond)) { PANIC("assertion failed: " #cond); } } while (0)
