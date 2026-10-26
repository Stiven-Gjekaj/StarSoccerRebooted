// ssr_test: the checks of the tests.
// Each test is a program. It returns 1 when a check fails.

#ifndef SSR_TEST_H
#define SSR_TEST_H

#include <stdio.h>

static int ssr_test_failures = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__,    \
                    #condition);                                                \
            ssr_test_failures++;                                                \
        }                                                                       \
    } while (0)

#define TEST_RESULT() (ssr_test_failures == 0 ? 0 : 1)

#endif
