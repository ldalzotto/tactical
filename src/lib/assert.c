#include "assert.h"

void panic(bool condition) {
    if (!condition) {
        __builtin_trap();
    }
}
