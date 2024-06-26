/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2024 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/nogg.h"

#define TEST(func)  extern int func(void);
#include "tests/coverage-tests.h"
#undef TEST

typedef int TestFunction(void);
static const struct {const char *name; TestFunction *f;} tests[] = {
#define TEST(func)  {#func, func},
#include "tests/coverage-tests.h"
#undef TEST
};


int main(int argc, char **argv)
{
    const bool verbose = (argc > 1 && strcmp(argv[1], "-v") == 0);

    const int num_tests = sizeof(tests) / sizeof(*tests);
    int failed = 0;
    for (int i = 0; i < num_tests; i++) {
        if (verbose) {
            printf("%s\n", tests[i].name);
        }
        const int result = (*tests[i].f)();
        if (result != EXIT_SUCCESS) {
            printf("FAILED: %s\n", tests[i].name);
            failed++;
        }
    }
    if (failed == 0) {
        printf("All tests passed.\n");
        return EXIT_SUCCESS;
    } else {
        const int passed = num_tests - failed;
        printf("%d test%s passed, %d test%s failed.\n",
               passed, passed==1 ? "" : "s", failed, failed==1 ? "" : "s");
        return EXIT_FAILURE;
    }
}
