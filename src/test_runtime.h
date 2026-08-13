#pragma once

#include "test.h"

extern const test_case_t g_runtime_tests[];
extern const uint32_t g_runtime_tests_count;

#ifdef APP_UNITY_BUILD
#include "test_runtime.c"
#endif
