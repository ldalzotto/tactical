#pragma once

__attribute__((import_module("env"), import_name("report_panic")))
extern void report_panic(const char *file, int file_len, int line, const char *msg, int msg_len);

int panic_strlen(const char *s);

__attribute__((noreturn))
void panic(const char *file, int line, const char *msg);

#ifndef NDEBUG
#define assert(cond, msg) do { if (!(cond)) { panic(__FILE__, __LINE__, (msg)); } } while (0)
#else
#define assert(cond, msg) ((void)0)
#endif
