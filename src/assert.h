#pragma once

#include <stdbool.h>

void panic(bool condition);

#ifndef NDEBUG
#define assert panic
#else
#define assert(...) ((void)0)
#endif
