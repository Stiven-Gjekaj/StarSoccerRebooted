// nsr_test: the checks of the tests.
// Each test is a program. It returns 1 when a check fails.

#ifndef NSR_TEST_H
#define NSR_TEST_H

#include <stdio.h>

static int nsr_test_failures = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__,    \
                    #condition);                                                \
            nsr_test_failures++;                                                \
        }                                                                       \
    } while (0)

#define TEST_RESULT() (nsr_test_failures == 0 ? 0 : 1)

#endif
