/**
 * @file auxbeam_light.c
 * @brief Main library implementation for AuxBeam light controller
 */

#include "auxbeam_light.h"
#include "hal_uart.h"
#include "command_queue.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "auxbeam_light";

// T022: Library context structure
struct auxbeam_context {
    hal_uart_handle_t *uart_handle;
    uint32_t default_timeout_ms;
    bool initialized;
};

// T024: Initialize library
auxbeam_error_t auxbeam_light_init(const auxbeam_config_t *config, auxbeam_context_t **ctx_out) {
    if (config == NULL || ctx_out == NULL) {
        return AUXBEAM_ERR_INVALID_PARAM;
    }

    // Allocate context
    auxbeam_context_t *ctx = (auxbeam_context_t *)malloc(sizeof(auxbeam_context_t));
    if (ctx == NULL) {
        return AUXBEAM_ERR_UART;
    }

    memset(ctx, 0, sizeof(auxbeam_context_t));
    ctx->default_timeout_ms = config->default_timeout_ms;

    // Initialize UART HAL
    hal_uart_config_t uart_config = {
        .uart_num = config->uart_num,
        .tx_pin = config->tx_pin,
        .rx_pin = config->rx_pin,
        .baud_rate = AUXBEAM_UART_BAUD_RATE
    };

    if (hal_uart_init(&uart_config, &ctx->uart_handle) != 0) {
        free(ctx);
        return AUXBEAM_ERR_UART;
    }

    // Initialize command queue
    command_queue_init();

    ctx->initialized = true;
    *ctx_out = ctx;

    ESP_LOGI(TAG, "Library initialized (UART%d, TX:%d, RX:%d, timeout:%ums)",
             config->uart_num, config->tx_pin, config->rx_pin, config->default_timeout_ms);

    return AUXBEAM_OK;
}

// T025: Deinitialize library
auxbeam_error_t auxbeam_light_deinit(auxbeam_context_t *ctx) {
    if (ctx == NULL) {
        return AUXBEAM_ERR_INVALID_PARAM;
    }

    if (ctx->uart_handle != NULL) {
        hal_uart_deinit(ctx->uart_handle);
    }

    ctx->initialized = false;
    free(ctx);

    ESP_LOGI(TAG, "Library deinitialized");
    return AUXBEAM_OK;
}

// T026-T032: Set single output
auxbeam_error_t auxbeam_light_set_output(
    auxbeam_context_t *ctx,
    uint8_t output_index,
    auxbeam_output_state_t state,
    auxbeam_ack_callback_t ack_cb,
    auxbeam_timeout_callback_t timeout_cb,
    void *user_data
) {
    // T027: Parameter validation
    if (ctx == NULL || !ctx->initialized) {
        return AUXBEAM_ERR_NOT_INITIALIZED;
    }

    if (output_index >= AUXBEAM_NUM_OUTPUTS) {
        return AUXBEAM_ERR_INVALID_PARAM;
    }

    if (state != AUXBEAM_OUTPUT_OFF && state != AUXBEAM_OUTPUT_ON &&
        state != AUXBEAM_OUTPUT_STROBE && state != AUXBEAM_OUTPUT_IGNORE) {
        return AUXBEAM_ERR_INVALID_PARAM;
    }

    // T028: Check queue availability
    if (command_queue_get_count() >= AUXBEAM_MAX_PENDING_COMMANDS) {
        return AUXBEAM_ERR_QUEUE_FULL;
    }

    // T029: Allocate pending command and get sequence ID
    uint8_t seq_id;
    pending_command_t *pending = command_queue_allocate(&seq_id);
    if (pending == NULL) {
        return AUXBEAM_ERR_QUEUE_FULL;
    }

    // T030: Build command message - single output, others IGNORE
    auxbeam_output_state_t states[AUXBEAM_NUM_OUTPUTS];
    for (int i = 0; i < AUXBEAM_NUM_OUTPUTS; i++) {
        states[i] = (i == output_index) ? state : AUXBEAM_OUTPUT_IGNORE;
    }

    auxbeam_message_t msg;
    light_protocol_encode_command(&msg, seq_id, states);

    // T031: Send command via HAL
    int sent = hal_uart_send(ctx->uart_handle, (const uint8_t *)&msg, sizeof(msg));
    if (sent != sizeof(msg)) {
        command_queue_free(seq_id);
        return AUXBEAM_ERR_UART;
    }

    // T032: Store pending command details
    memcpy(pending->outputs, states, sizeof(states));
    pending->send_time_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    pending->timeout_ms = ctx->default_timeout_ms;
    pending->ack_cb = ack_cb;
    pending->timeout_cb = timeout_cb;
    pending->user_data = user_data;

    ESP_LOGD(TAG, "Command sent: seq_id=%d, output=%d, state=%d", seq_id, output_index, state);

    return AUXBEAM_OK;
}

// T033-T040: Process acknowledgments and timeouts
auxbeam_error_t auxbeam_light_process(auxbeam_context_t *ctx) {
    if (ctx == NULL || !ctx->initialized) {
        return AUXBEAM_ERR_NOT_INITIALIZED;
    }

    // T033: Try to receive ACK message
    auxbeam_message_t msg;
    int received = hal_uart_receive(ctx->uart_handle, (uint8_t *)&msg, sizeof(msg), 0);

    if (received == sizeof(msg)) {
        // T034: Validate checksum
        uint8_t seq_id;
        if (!light_protocol_decode_ack(&msg, &seq_id)) {
            ESP_LOGW(TAG, "Invalid ACK received (bad checksum or format)");
            // Don't return error - continue processing
        } else {
            // T035: Find pending command by sequence ID
            pending_command_t *pending = command_queue_find(seq_id);

            if (pending != NULL) {
                // T036: Invoke acknowledgment callback
                if (pending->ack_cb != NULL) {
                    pending->ack_cb(seq_id, pending->user_data);
                }

                ESP_LOGD(TAG, "ACK received: seq_id=%d", seq_id);

                // T037: Free pending command slot
                command_queue_free(seq_id);
            } else {
                ESP_LOGW(TAG, "ACK for unknown seq_id=%d", seq_id);
            }
        }
    }

    // T038-T040: Check for timeouts
    command_queue_check_timeouts();

    return AUXBEAM_OK;
}

// User Story 2: Set multiple outputs
auxbeam_error_t auxbeam_light_set_multiple(
    auxbeam_context_t *ctx,
    const auxbeam_output_state_t states[6],
    auxbeam_ack_callback_t ack_cb,
    auxbeam_timeout_callback_t timeout_cb,
    void *user_data
) {
    if (ctx == NULL || !ctx->initialized) {
        return AUXBEAM_ERR_NOT_INITIALIZED;
    }

    if (states == NULL) {
        return AUXBEAM_ERR_INVALID_PARAM;
    }

    // Validate all states
    for (int i = 0; i < AUXBEAM_NUM_OUTPUTS; i++) {
        if (states[i] != AUXBEAM_OUTPUT_OFF && states[i] != AUXBEAM_OUTPUT_ON &&
            states[i] != AUXBEAM_OUTPUT_STROBE && states[i] != AUXBEAM_OUTPUT_IGNORE) {
            return AUXBEAM_ERR_INVALID_PARAM;
        }
    }

    // Check queue availability
    if (command_queue_get_count() >= AUXBEAM_MAX_PENDING_COMMANDS) {
        return AUXBEAM_ERR_QUEUE_FULL;
    }

    // Allocate pending command
    uint8_t seq_id;
    pending_command_t *pending = command_queue_allocate(&seq_id);
    if (pending == NULL) {
        return AUXBEAM_ERR_QUEUE_FULL;
    }

    // Build command message with all output states
    auxbeam_message_t msg;
    light_protocol_encode_command(&msg, seq_id, states);

    // Send command
    int sent = hal_uart_send(ctx->uart_handle, (const uint8_t *)&msg, sizeof(msg));
    if (sent != sizeof(msg)) {
        command_queue_free(seq_id);
        return AUXBEAM_ERR_UART;
    }

    // Store pending command details
    memcpy(pending->outputs, states, sizeof(auxbeam_output_state_t) * AUXBEAM_NUM_OUTPUTS);
    pending->send_time_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    pending->timeout_ms = ctx->default_timeout_ms;
    pending->ack_cb = ack_cb;
    pending->timeout_cb = timeout_cb;
    pending->user_data = user_data;

    ESP_LOGD(TAG, "Multi-output command sent: seq_id=%d", seq_id);

    return AUXBEAM_OK;
}

// User Story 3: Utility functions
auxbeam_error_t auxbeam_light_set_timeout(auxbeam_context_t *ctx, uint32_t timeout_ms) {
    if (ctx == NULL || !ctx->initialized) {
        return AUXBEAM_ERR_NOT_INITIALIZED;
    }

    if (timeout_ms == 0) {
        return AUXBEAM_ERR_INVALID_PARAM;
    }

    ctx->default_timeout_ms = timeout_ms;
    return AUXBEAM_OK;
}

auxbeam_error_t auxbeam_light_get_pending_count(auxbeam_context_t *ctx, uint32_t *count_out) {
    if (ctx == NULL || !ctx->initialized || count_out == NULL) {
        return AUXBEAM_ERR_INVALID_PARAM;
    }

    *count_out = command_queue_get_count();
    return AUXBEAM_OK;
}
