#include "panic.h"

int panic_strlen(const char *s) {
    int len = 0;
    while (s[len]) {
        len++;
    }
    return len;
}

__attribute__((noreturn))
void panic(const char *file, int line, const char *msg) {
    report_panic(file, panic_strlen(file), line, msg, panic_strlen(msg));
    __builtin_trap();
}
