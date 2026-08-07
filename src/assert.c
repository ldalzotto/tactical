#include "assert.h"

void panic(unsigned char condition) {
    if (!condition) {
        __builtin_trap();
    }
}
