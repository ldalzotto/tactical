#pragma once

void panic(unsigned char condition);

#ifndef NDEBUG
#define assert panic
#else
#define assert ((void)0)
#endif
