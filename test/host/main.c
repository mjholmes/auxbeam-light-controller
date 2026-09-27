/**
 * @file main.c
 * @brief Host test runner entry point: runs every test suite and reports
 * a single pass/fail summary and exit code, per quickstart.md's
 * "Testing Without Hardware" section (`./test_runner`).
 */
#include "test_util.h"
#include <stdio.h>

void run_checksum_tests(void);
void run_protocol_tests(void);
void run_queue_tests(void);
void run_integration_tests(void);

int main(void) {
    run_checksum_tests();
    run_protocol_tests();
    run_queue_tests();
    run_integration_tests();

    printf("\n%d/%d checks passed\n", g_test_count - g_test_failures, g_test_count);
    if (g_test_failures > 0) {
        printf("%d FAILED\n", g_test_failures);
        return 1;
    }
    printf("ALL PASSED\n");
    return 0;
}
