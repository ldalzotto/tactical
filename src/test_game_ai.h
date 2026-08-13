#pragma once

#include "test.h"

extern const test_case_t g_game_ai_tests[];
extern const uint32_t g_game_ai_tests_count;

#ifdef APP_UNITY_BUILD
#include "test_game_ai.c"
#endif
