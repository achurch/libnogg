/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include <stdio.h>
#include <stdlib.h>

#define TEST(func)  extern int func(void);
#include "tests/coverage-tests.h"
#undef TEST

typedef int TestFunction(void);
static TestFunction * const tests[] = {
#define TEST(func)  func,
#include "tests/coverage-tests.h"
#undef TEST
};


int main(void)
{
    const int num_tests = sizeof(tests) / sizeof(*tests);
    int failed = 0;
    for (int i = 0; i < num_tests; i++) {
        const int result = (*tests[i])();
        if (result != EXIT_SUCCESS) {
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