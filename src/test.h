#pragma once

#include "lib/memory.h"
#include <stdint.h>

typedef void (*test_fn_t)(void);

typedef struct {
    slice_t name;
    test_fn_t fn;
} test_case_t;

uint32_t test_discovery_count(void);
const char *test_discovery_name_begin(uint32_t index);
const char *test_discovery_name_end(uint32_t index);
test_fn_t test_discovery_fn_at(uint32_t index);

void test_run(test_fn_t fn);
