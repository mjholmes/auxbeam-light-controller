/**
 * @file hal_uart.h
 * @brief Hardware Abstraction Layer for UART operations
 *
 * Defines platform-independent UART interface. ESP-IDF implementation
 * is in hal_uart_esp32.c. Mock implementation for host testing is in
 * test/host/mock_hal_uart.c.
 */

#ifndef HAL_UART_H
#define HAL_UART_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque UART handle type
 */
typedef struct hal_uart_handle hal_uart_handle_t;

/**
 * @brief UART configuration parameters
 */
typedef struct {
    int uart_num;       /**< UART port number */
    int tx_pin;         /**< TX GPIO pin */
    int rx_pin;         /**< RX GPIO pin */
    uint32_t baud_rate; /**< Baud rate (e.g., 2400) */
} hal_uart_config_t;

/**
 * @brief Initialize UART hardware
 *
 * @param[in] config UART configuration
 * @param[out] handle_out Pointer to receive UART handle
 * @return 0 on success, negative on error
 */
int hal_uart_init(const hal_uart_config_t *config, hal_uart_handle_t **handle_out);

/**
 * @brief Deinitialize UART hardware
 *
 * @param[in] handle UART handle
 * @return 0 on success, negative on error
 */
int hal_uart_deinit(hal_uart_handle_t *handle);

/**
 * @brief Send bytes over UART
 *
 * @param[in] handle UART handle
 * @param[in] data Data to send
 * @param[in] len Number of bytes to send
 * @return Number of bytes sent, or negative on error
 */
int hal_uart_send(hal_uart_handle_t *handle, const uint8_t *data, size_t len);

/**
 * @brief Receive bytes from UART (non-blocking)
 *
 * @param[in] handle UART handle
 * @param[out] data Buffer to store received data
 * @param[in] max_len Maximum bytes to read
 * @param[in] timeout_ms Timeout in milliseconds (0 = non-blocking)
 * @return Number of bytes received, or negative on error
 */
int hal_uart_receive(hal_uart_handle_t *handle, uint8_t *data, size_t max_len, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* HAL_UART_H */
