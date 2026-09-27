/**
 * @file test_queue.c
 * @brief Unit tests for command_queue.c, independent of auxbeam_light.c
 * or any UART HAL.
 */
#include "command_queue.h"
#include "mock_hal_uart.h"
#include "test_util.h"

static int g_ack_calls = 0;
static int g_timeout_calls = 0;
static uint8_t g_last_seq_id = 0;

static void ack_cb(uint8_t seq_id, void *user_data) {
    (void)user_data;
    g_ack_calls++;
    g_last_seq_id = seq_id;
}

static void timeout_cb(uint8_t seq_id, auxbeam_error_t error, void *user_data) {
    (void)user_data;
    (void)error;
    g_timeout_calls++;
    g_last_seq_id = seq_id;
}

static void reset_counters(void) {
    g_ack_calls = 0;
    g_timeout_calls = 0;
    mock_clock_set_ms(0);
}

// AUXBEAM_MAX_PENDING_COMMANDS (4): allocate exactly that many, then verify
// the queue reports full.
static void test_allocate_and_queue_full(void) {
    command_queue_init();
    reset_counters();

    uint8_t seq_ids[4];
    for (int i = 0; i < 4; i++) {
        pending_command_t *cmd = command_queue_allocate(&seq_ids[i]);
        TEST_CHECK(cmd != NULL);
    }
    TEST_CHECK_EQ(command_queue_get_count(), 4);

    uint8_t overflow_seq;
    TEST_CHECK(command_queue_allocate(&overflow_seq) == NULL);

    command_queue_cancel_all();
}

// data-model.md invariant: next_seq_id wraps from 0xFF back to 0x00.
// Allocations 0..255 consume seq_id 0 through 255; the 257th allocation
// is the first one to observe the wrap back to 0.
static void test_seq_id_wraps(void) {
    command_queue_init();
    reset_counters();

    uint8_t seq_id = 0;
    for (int i = 0; i < 256; i++) {
        pending_command_t *cmd = command_queue_allocate(&seq_id);
        TEST_CHECK(cmd != NULL);
        TEST_CHECK_EQ(seq_id, i);
        command_queue_free(seq_id);
    }

    pending_command_t *cmd = command_queue_allocate(&seq_id);
    TEST_CHECK(cmd != NULL);
    TEST_CHECK_EQ(seq_id, 0);  // wrapped

    command_queue_cancel_all();
}

static void test_find_and_free(void) {
    command_queue_init();
    reset_counters();

    uint8_t seq_id;
    pending_command_t *cmd = command_queue_allocate(&seq_id);
    TEST_CHECK(cmd != NULL);

    TEST_CHECK(command_queue_find(seq_id) == cmd);
    TEST_CHECK(command_queue_find((uint8_t)(seq_id + 1)) == NULL);  // unknown seq_id

    command_queue_free(seq_id);
    TEST_CHECK_EQ(command_queue_get_count(), 0);
    TEST_CHECK(command_queue_find(seq_id) == NULL);

    // Freeing an already-freed / unknown seq_id must be a safe no-op
    // (pending_count must not underflow).
    command_queue_free(seq_id);
    TEST_CHECK_EQ(command_queue_get_count(), 0);

    command_queue_cancel_all();
}

static void test_check_timeouts(void) {
    command_queue_init();
    reset_counters();

    uint8_t seq_id;
    pending_command_t *cmd = command_queue_allocate(&seq_id);
    cmd->send_time_ms = 0;
    cmd->timeout_ms = 100;
    cmd->ack_cb = ack_cb;
    cmd->timeout_cb = timeout_cb;

    mock_clock_set_ms(50);
    command_queue_check_timeouts();
    TEST_CHECK_EQ(g_timeout_calls, 0);  // not yet expired
    TEST_CHECK_EQ(command_queue_get_count(), 1);

    mock_clock_set_ms(100);
    command_queue_check_timeouts();
    TEST_CHECK_EQ(g_timeout_calls, 1);
    TEST_CHECK_EQ(g_last_seq_id, seq_id);
    TEST_CHECK_EQ(command_queue_get_count(), 0);  // slot freed

    command_queue_cancel_all();
}

// cancel_all() must not invoke callbacks (contrast with check_timeouts()).
static void test_cancel_all_no_callbacks(void) {
    command_queue_init();
    reset_counters();

    uint8_t seq_id;
    pending_command_t *cmd = command_queue_allocate(&seq_id);
    cmd->ack_cb = ack_cb;
    cmd->timeout_cb = timeout_cb;

    command_queue_cancel_all();

    TEST_CHECK_EQ(command_queue_get_count(), 0);
    TEST_CHECK_EQ(g_ack_calls, 0);
    TEST_CHECK_EQ(g_timeout_calls, 0);
}

static pending_command_t *g_reentrant_alloc_result;
static bool g_reentrant_alloc_attempted;

static void reentrant_retry_timeout_cb(uint8_t seq_id, auxbeam_error_t error, void *user_data) {
    (void)seq_id;
    (void)error;
    (void)user_data;
    g_timeout_calls++;
    g_reentrant_alloc_attempted = true;
    uint8_t new_seq_id;
    // This is the L-2 regression: at the point this callback runs, the
    // slot that just timed out must already be free, so this reentrant
    // allocate (standing in for a retry via set_output()) must succeed
    // even though all 4 slots were in use a moment ago.
    g_reentrant_alloc_result = command_queue_allocate(&new_seq_id);
}

static void test_timeout_callback_can_reentrantly_allocate(void) {
    command_queue_init();
    reset_counters();
    g_reentrant_alloc_result = NULL;
    g_reentrant_alloc_attempted = false;

    // Fill all 4 slots; only slot 0 is due to expire at clock=100.
    for (int i = 0; i < 4; i++) {
        uint8_t seq_id;
        pending_command_t *cmd = command_queue_allocate(&seq_id);
        TEST_CHECK(cmd != NULL);
        cmd->send_time_ms = 0;
        if (i == 0) {
            cmd->timeout_ms = 100;
            cmd->timeout_cb = reentrant_retry_timeout_cb;
        } else {
            cmd->timeout_ms = 10000;  // must not expire in this test
            cmd->timeout_cb = timeout_cb;
        }
    }
    TEST_CHECK_EQ(command_queue_get_count(), 4);

    mock_clock_set_ms(100);
    command_queue_check_timeouts();

    TEST_CHECK(g_reentrant_alloc_attempted);
    TEST_CHECK(g_reentrant_alloc_result != NULL);  // must not be QUEUE_FULL
    TEST_CHECK_EQ(g_timeout_calls, 1);  // only slot 0 timed out
    // Slot 0 was freed then immediately reclaimed by the reentrant
    // allocate, so the count is back to 4, not 3.
    TEST_CHECK_EQ(command_queue_get_count(), 4);

    command_queue_cancel_all();
}

void run_queue_tests(void) {
    test_allocate_and_queue_full();
    test_seq_id_wraps();
    test_find_and_free();
    test_check_timeouts();
    test_cancel_all_no_callbacks();
    test_timeout_callback_can_reentrantly_allocate();
}
