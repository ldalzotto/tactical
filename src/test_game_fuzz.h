#pragma once

#include "test.h"

extern const test_case_t g_game_fuzz_tests[];
extern const uint32_t g_game_fuzz_tests_count;

#ifdef APP_UNITY_BUILD
#include "test_game_fuzz.c"
#endif
