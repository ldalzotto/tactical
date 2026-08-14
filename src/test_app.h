#pragma once

#include "test.h"

extern const test_case_t g_app_tests[];
extern const uint32_t g_app_tests_count;

#ifdef APP_UNITY_BUILD
#include "test_app.c"
#endif
