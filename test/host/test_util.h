/**
 * @file test_util.h
 * @brief Minimal dependency-free assertion macros for host tests.
 *
 * plan.md's Technical Context named Unity + CMock, but pulling in that
 * framework (and Ruby/Ceedling for CMock's mock generation) is a heavy,
 * non-trivial dependency for what these tests need. This is a deliberate,
 * documented deviation: a small custom harness in the spirit of the
 * constitution's "minimal dependencies" principle, not an oversight.
 */
#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <stdio.h>

extern int g_test_count;
extern int g_test_failures;

#define TEST_CHECK(cond) \
    do { \
        g_test_count++; \
        if (!(cond)) { \
            g_test_failures++; \
            printf("FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while (0)

#define TEST_CHECK_EQ(actual, expected) \
    do { \
        g_test_count++; \
        long _auxbeam_a = (long)(actual); \
        long _auxbeam_e = (long)(expected); \
        if (_auxbeam_a != _auxbeam_e) { \
            g_test_failures++; \
            printf("FAIL: %s:%d: expected %ld, got %ld\n", __FILE__, __LINE__, _auxbeam_e, _auxbeam_a); \
        } \
    } while (0)

#endif /* TEST_UTIL_H */
