/**
 * @file test_integration.c
 * @brief End-to-end tests driving the real public API against the mock
 * UART HAL -- no ESP32 or RA60 controller required.
 */
#include "auxbeam_light.h"
#include "light_protocol.h"
#include "mock_hal_uart.h"
#include "test_util.h"
#include <string.h>

static int g_ack_seen;
static int g_timeout_seen;
static uint8_t g_last_acked_seq;

static void on_ack(uint8_t seq_id, void *user_data) {
    (void)user_data;
    g_ack_seen++;
    g_last_acked_seq = seq_id;
}

static void on_timeout(uint8_t seq_id, auxbeam_error_t error, void *user_data) {
    (void)user_data;
    (void)error;
    (void)seq_id;
    g_timeout_seen++;
}

static void reset_all(void) {
    mock_hal_uart_reset();
    g_ack_seen = 0;
    g_timeout_seen = 0;
}

static void build_ack_frame(uint8_t seq_id, uint8_t out[11]) {
    out[0] = seq_id;
    out[1] = 0xFF;
    out[2] = 0xF8;
    memset(out + 3, 0, 7);
    out[10] = light_protocol_calculate_checksum(out, 10);
}

// Spec.md US1 AC1/AC4: a command gets acknowledged, and a split frame
// (routine at 2400 baud) still decodes correctly across process() calls --
// the regression check for the framing-desync bug fixed this session.
static void test_ack_split_across_process_calls(void) {
    reset_all();
    auxbeam_context_t *ctx = NULL;
    auxbeam_config_t config = {.uart_num = 1, .tx_pin = 17, .rx_pin = 16, .default_timeout_ms = 500};
    TEST_CHECK_EQ(auxbeam_light_init(&config, &ctx), AUXBEAM_OK);

    // Double-init must be rejected (single-instance guard).
    auxbeam_context_t *ctx2 = NULL;
    TEST_CHECK_EQ(auxbeam_light_init(&config, &ctx2), AUXBEAM_ERR_ALREADY_INITIALIZED);

    TEST_CHECK_EQ(auxbeam_light_set_output(ctx, 0, AUXBEAM_OUTPUT_ON, on_ack, on_timeout, NULL), AUXBEAM_OK);
    TEST_CHECK_EQ(mock_hal_uart_tx_len(), 11);

    uint8_t ack[11];
    build_ack_frame(0, ack);
    mock_hal_uart_push_rx(ack, 5);
    auxbeam_light_process(ctx);
    TEST_CHECK_EQ(g_ack_seen, 0);  // partial frame must not decode yet

    mock_hal_uart_push_rx(ack + 5, 6);
    auxbeam_light_process(ctx);
    TEST_CHECK_EQ(g_ack_seen, 1);
    TEST_CHECK_EQ(g_last_acked_seq, 0);

    uint32_t pending = 123;
    auxbeam_light_get_pending_count(ctx, &pending);
    TEST_CHECK_EQ(pending, 0);

    auxbeam_light_deinit(ctx);
}

// Spec.md US1 AC4 / US3 AC1: no ACK within the timeout invokes the timeout callback.
static void test_timeout_path(void) {
    reset_all();
    auxbeam_context_t *ctx = NULL;
    auxbeam_config_t config = {.uart_num = 1, .tx_pin = 17, .rx_pin = 16, .default_timeout_ms = 500};
    auxbeam_light_init(&config, &ctx);

    TEST_CHECK_EQ(auxbeam_light_set_output(ctx, 1, AUXBEAM_OUTPUT_OFF, on_ack, on_timeout, NULL), AUXBEAM_OK);
    mock_clock_advance_ms(600);
    auxbeam_light_process(ctx);
    TEST_CHECK_EQ(g_timeout_seen, 1);
    TEST_CHECK_EQ(g_ack_seen, 0);

    auxbeam_light_deinit(ctx);
}

// Spec.md US3 AC2: a corrupted (bad-checksum) ACK is rejected; the command
// stays pending until a valid ACK (or timeout) resolves it.
static void test_bad_checksum_ack_rejected(void) {
    reset_all();
    auxbeam_context_t *ctx = NULL;
    auxbeam_config_t config = {.uart_num = 1, .tx_pin = 17, .rx_pin = 16, .default_timeout_ms = 500};
    auxbeam_light_init(&config, &ctx);

    auxbeam_light_set_output(ctx, 0, AUXBEAM_OUTPUT_ON, on_ack, on_timeout, NULL);

    uint8_t bad_ack[11];
    build_ack_frame(0, bad_ack);
    bad_ack[10] ^= 0xFF;  // corrupt the checksum
    mock_hal_uart_push_rx(bad_ack, sizeof(bad_ack));
    auxbeam_light_process(ctx);
    TEST_CHECK_EQ(g_ack_seen, 0);

    uint32_t pending = 0;
    auxbeam_light_get_pending_count(ctx, &pending);
    TEST_CHECK_EQ(pending, 1);  // still pending -- rejected, not matched

    uint8_t good_ack[11];
    build_ack_frame(0, good_ack);
    mock_hal_uart_push_rx(good_ack, sizeof(good_ack));
    auxbeam_light_process(ctx);
    TEST_CHECK_EQ(g_ack_seen, 1);

    auxbeam_light_deinit(ctx);
}

// Spec.md US3 AC3: ACKs arriving out of order are matched by sequence ID,
// not by send order.
static void test_out_of_order_acks(void) {
    reset_all();
    auxbeam_context_t *ctx = NULL;
    auxbeam_config_t config = {.uart_num = 1, .tx_pin = 17, .rx_pin = 16, .default_timeout_ms = 500};
    auxbeam_light_init(&config, &ctx);

    auxbeam_light_set_output(ctx, 0, AUXBEAM_OUTPUT_ON, on_ack, on_timeout, NULL);   // seq 0
    auxbeam_light_set_output(ctx, 1, AUXBEAM_OUTPUT_OFF, on_ack, on_timeout, NULL);  // seq 1

    uint8_t ack1[11];
    build_ack_frame(1, ack1);
    mock_hal_uart_push_rx(ack1, sizeof(ack1));
    auxbeam_light_process(ctx);
    TEST_CHECK_EQ(g_ack_seen, 1);
    TEST_CHECK_EQ(g_last_acked_seq, 1);

    uint32_t pending = 0;
    auxbeam_light_get_pending_count(ctx, &pending);
    TEST_CHECK_EQ(pending, 1);  // seq 0 still outstanding

    uint8_t ack0[11];
    build_ack_frame(0, ack0);
    mock_hal_uart_push_rx(ack0, sizeof(ack0));
    auxbeam_light_process(ctx);
    TEST_CHECK_EQ(g_ack_seen, 2);
    TEST_CHECK_EQ(g_last_acked_seq, 0);

    auxbeam_light_deinit(ctx);
}

// Spec.md US2: set_multiple() commands several outputs in one frame.
static void test_set_multiple(void) {
    reset_all();
    auxbeam_context_t *ctx = NULL;
    auxbeam_config_t config = {.uart_num = 1, .tx_pin = 17, .rx_pin = 16, .default_timeout_ms = 500};
    auxbeam_light_init(&config, &ctx);

    auxbeam_output_state_t states[6] = {
        AUXBEAM_OUTPUT_ON, AUXBEAM_OUTPUT_STROBE, AUXBEAM_OUTPUT_OFF,
        AUXBEAM_OUTPUT_IGNORE, AUXBEAM_OUTPUT_IGNORE, AUXBEAM_OUTPUT_IGNORE,
    };
    TEST_CHECK_EQ(auxbeam_light_set_multiple(ctx, states, on_ack, on_timeout, NULL), AUXBEAM_OK);
    TEST_CHECK_EQ(mock_hal_uart_tx_len(), 11);  // one frame, not six

    uint8_t ack[11];
    build_ack_frame(0, ack);
    mock_hal_uart_push_rx(ack, sizeof(ack));
    auxbeam_light_process(ctx);
    TEST_CHECK_EQ(g_ack_seen, 1);

    auxbeam_light_deinit(ctx);
}

// AUXBEAM_MAX_PENDING_COMMANDS (4) via the public API.
static void test_queue_full_via_api(void) {
    reset_all();
    auxbeam_context_t *ctx = NULL;
    auxbeam_config_t config = {.uart_num = 1, .tx_pin = 17, .rx_pin = 16, .default_timeout_ms = 500};
    auxbeam_light_init(&config, &ctx);

    for (int i = 0; i < AUXBEAM_MAX_PENDING_COMMANDS; i++) {
        TEST_CHECK_EQ(auxbeam_light_set_output(ctx, 0, AUXBEAM_OUTPUT_ON, on_ack, on_timeout, NULL), AUXBEAM_OK);
    }
    TEST_CHECK_EQ(auxbeam_light_set_output(ctx, 0, AUXBEAM_OUTPUT_ON, on_ack, on_timeout, NULL), AUXBEAM_ERR_QUEUE_FULL);

    auxbeam_light_deinit(ctx);
}

// deinit() must cancel pending commands without invoking their callbacks
// (contracts/api.h's documented behavior, fixed this session).
static void test_deinit_cancels_without_callback(void) {
    reset_all();
    auxbeam_context_t *ctx = NULL;
    auxbeam_config_t config = {.uart_num = 1, .tx_pin = 17, .rx_pin = 16, .default_timeout_ms = 500};
    auxbeam_light_init(&config, &ctx);

    auxbeam_light_set_output(ctx, 2, AUXBEAM_OUTPUT_ON, on_ack, on_timeout, NULL);
    TEST_CHECK_EQ(auxbeam_light_deinit(ctx), AUXBEAM_OK);
    TEST_CHECK_EQ(g_ack_seen, 0);
    TEST_CHECK_EQ(g_timeout_seen, 0);
}

void run_integration_tests(void) {
    test_ack_split_across_process_calls();
    test_timeout_path();
    test_bad_checksum_ack_rejected();
    test_out_of_order_acks();
    test_set_multiple();
    test_queue_full_via_api();
    test_deinit_cancels_without_callback();
}
