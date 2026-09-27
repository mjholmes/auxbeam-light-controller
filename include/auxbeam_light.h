/**
 * @file auxbeam_light.h
 * @brief Public API for AuxBeam Light Controller Library
 *
 * This is the main header file for controlling an RA60 light controller
 * via 2400 baud UART on ESP32.
 */

#ifndef AUXBEAM_LIGHT_H
#define AUXBEAM_LIGHT_H

#include <stdint.h>
#include <stdbool.h>
#include "light_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Error codes returned by API functions
 */
typedef enum {
    AUXBEAM_OK = 0,                    /**< Success */
    AUXBEAM_ERR_INVALID_PARAM = -1,    /**< Invalid parameter */
    AUXBEAM_ERR_NOT_INITIALIZED = -2,  /**< Library not initialized */
    AUXBEAM_ERR_QUEUE_FULL = -3,       /**< Too many pending commands (max 4) */
    AUXBEAM_ERR_UART = -4,             /**< UART hardware error */
    AUXBEAM_ERR_TIMEOUT = -5,          /**< Command timeout */
    AUXBEAM_ERR_BAD_CHECKSUM = -6,     /**< Checksum validation failed */
} auxbeam_error_t;

/**
 * @brief Acknowledgment callback function type
 */
typedef void (*auxbeam_ack_callback_t)(uint8_t seq_id, void *user_data);

/**
 * @brief Timeout/error callback function type
 */
typedef void (*auxbeam_timeout_callback_t)(uint8_t seq_id, auxbeam_error_t error, void *user_data);

/**
 * @brief Opaque library context handle
 */
typedef struct auxbeam_context auxbeam_context_t;

/**
 * @brief Initialization configuration
 */
typedef struct {
    int uart_num;                    /**< UART port number (e.g., UART_NUM_1) */
    int tx_pin;                      /**< GPIO pin number for TX */
    int rx_pin;                      /**< GPIO pin number for RX */
    uint32_t default_timeout_ms;     /**< Default timeout in milliseconds */
} auxbeam_config_t;

/* API Functions */

/**
 * @brief Initialize the light controller library
 */
auxbeam_error_t auxbeam_light_init(const auxbeam_config_t *config, auxbeam_context_t **ctx_out);

/**
 * @brief Deinitialize the light controller library
 */
auxbeam_error_t auxbeam_light_deinit(auxbeam_context_t *ctx);

/**
 * @brief Set a single output to a specific state
 */
auxbeam_error_t auxbeam_light_set_output(
    auxbeam_context_t *ctx,
    uint8_t output_index,
    auxbeam_output_state_t state,
    auxbeam_ack_callback_t ack_cb,
    auxbeam_timeout_callback_t timeout_cb,
    void *user_data
);

/**
 * @brief Set multiple outputs to specific states
 */
auxbeam_error_t auxbeam_light_set_multiple(
    auxbeam_context_t *ctx,
    const auxbeam_output_state_t states[6],
    auxbeam_ack_callback_t ack_cb,
    auxbeam_timeout_callback_t timeout_cb,
    void *user_data
);

/**
 * @brief Process received acknowledgments and check for timeouts
 */
auxbeam_error_t auxbeam_light_process(auxbeam_context_t *ctx);

/**
 * @brief Set the default timeout for future commands
 */
auxbeam_error_t auxbeam_light_set_timeout(auxbeam_context_t *ctx, uint32_t timeout_ms);

/**
 * @brief Get the number of pending commands
 */
auxbeam_error_t auxbeam_light_get_pending_count(auxbeam_context_t *ctx, uint32_t *count_out);

/* Constants */

#define AUXBEAM_MAX_PENDING_COMMANDS 4
#define AUXBEAM_NUM_OUTPUTS 6
#define AUXBEAM_DEFAULT_TIMEOUT_MS 500
#define AUXBEAM_UART_BAUD_RATE 2400

#ifdef __cplusplus
}
#endif

#endif /* AUXBEAM_LIGHT_H */
