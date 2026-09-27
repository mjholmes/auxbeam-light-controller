/**
 * @file smoke_main.c
 * @brief Host build smoke check: links the real production sources against
 * the mock HAL and exercises the init -> send -> process -> deinit path.
 *
 * This is a scaffolding step (Build), not the full test suite (Tests) --
 * it's here to prove the component actually compiles and runs end to end
 * off-target, and along the way it exercises the exact bug this session
 * fixed: an ACK frame arriving in more than one hal_uart_receive() chunk,
 * split across two auxbeam_light_process() calls.
 */
#include "auxbeam_light.h"
#include "light_protocol.h"
#include "mock_hal_uart.h"
#include <stdio.h>
#include <string.h>

static int g_ack_seen = 0;
static int g_timeout_seen = 0;

static void on_ack(uint8_t seq_id, void *user_data) {
    (void)user_data;
    printf("on_ack: seq_id=%d\n", seq_id);
    g_ack_seen = 1;
}

static void on_timeout(uint8_t seq_id, auxbeam_error_t error, void *user_data) {
    (void)user_data;
    printf("on_timeout: seq_id=%d error=%d\n", seq_id, error);
    g_timeout_seen = 1;
}

// Builds a valid 11-byte ACK frame for the given sequence ID, using the
// real checksum function so the test frame matches production encoding.
static void build_ack_frame(uint8_t seq_id, uint8_t out[11]) {
    out[0] = seq_id;
    out[1] = 0xFF;  // msg_type: ACK
    out[2] = 0xF8;  // cmd_type: ACK, output command
    out[3] = 0x00;  // unknown, always 0x00 for ACKs
    out[4] = 0x00;
    out[5] = 0x00;
    out[6] = 0x00;
    out[7] = 0x00;
    out[8] = 0x00;
    out[9] = 0x00;
    out[10] = light_protocol_calculate_checksum(out, 10);
}

int main(void) {
    int failures = 0;

    mock_hal_uart_reset();

    auxbeam_config_t config = {
        .uart_num = 1,
        .tx_pin = 17,
        .rx_pin = 16,
        .default_timeout_ms = 500,
    };

    auxbeam_context_t *ctx = NULL;
    if (auxbeam_light_init(&config, &ctx) != AUXBEAM_OK) {
        printf("FAIL: init\n");
        return 1;
    }

    // A second init before deinit must be rejected (single-instance guard).
    auxbeam_context_t *ctx2 = NULL;
    if (auxbeam_light_init(&config, &ctx2) != AUXBEAM_ERR_ALREADY_INITIALIZED) {
        printf("FAIL: double-init was not rejected\n");
        failures++;
    }

    if (auxbeam_light_set_output(ctx, 0, AUXBEAM_OUTPUT_ON, on_ack, on_timeout, NULL) != AUXBEAM_OK) {
        printf("FAIL: set_output\n");
        failures++;
    }

    if (mock_hal_uart_tx_len() != 11) {
        printf("FAIL: expected 11 TX bytes, got %zu\n", mock_hal_uart_tx_len());
        failures++;
    }

    // Regression check for the framing-desync bug: split the ACK across
    // two process() calls, as would routinely happen at 2400 baud with the
    // documented 10ms polling interval.
    uint8_t ack[11];
    build_ack_frame(0, ack);
    mock_hal_uart_push_rx(ack, 5);       // first chunk: partial frame
    auxbeam_light_process(ctx);
    if (g_ack_seen) {
        printf("FAIL: ack fired on a partial frame\n");
        failures++;
    }

    mock_hal_uart_push_rx(ack + 5, 6);   // remaining chunk completes the frame
    auxbeam_light_process(ctx);
    if (!g_ack_seen) {
        printf("FAIL: ack callback did not fire once the frame completed\n");
        failures++;
    }

    uint32_t pending = 0;
    auxbeam_light_get_pending_count(ctx, &pending);
    if (pending != 0) {
        printf("FAIL: expected 0 pending after ACK, got %u\n", pending);
        failures++;
    }

    // Timeout path: send another command, advance the mock clock past the
    // timeout without ever pushing an ACK.
    g_ack_seen = 0;
    mock_hal_uart_clear_tx();
    if (auxbeam_light_set_output(ctx, 1, AUXBEAM_OUTPUT_OFF, on_ack, on_timeout, NULL) != AUXBEAM_OK) {
        printf("FAIL: second set_output\n");
        failures++;
    }
    mock_clock_advance_ms(600);
    auxbeam_light_process(ctx);
    if (!g_timeout_seen) {
        printf("FAIL: timeout callback did not fire\n");
        failures++;
    }

    // deinit() must cancel any still-pending command without invoking its callback.
    g_ack_seen = 0;
    g_timeout_seen = 0;
    auxbeam_light_set_output(ctx, 2, AUXBEAM_OUTPUT_ON, on_ack, on_timeout, NULL);
    if (auxbeam_light_deinit(ctx) != AUXBEAM_OK) {
        printf("FAIL: deinit\n");
        failures++;
    }
    if (g_ack_seen || g_timeout_seen) {
        printf("FAIL: deinit invoked a callback for a cancelled command\n");
        failures++;
    }

    if (failures == 0) {
        printf("OK: all smoke checks passed\n");
    } else {
        printf("FAILED: %d smoke check(s) failed\n", failures);
    }
    return failures == 0 ? 0 : 1;
}
