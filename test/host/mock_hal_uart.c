/**
 * @file mock_hal_uart.c
 * @brief Host-side mock implementation of hal_uart.h (no real hardware).
 */
#include "hal_uart.h"
#include "mock_hal_uart.h"
#include <string.h>

#define MOCK_RX_BUF_SIZE 256
#define MOCK_TX_BUF_SIZE 256

struct hal_uart_handle {
    int uart_num;
};

static struct hal_uart_handle g_handle;
static uint8_t g_rx_buf[MOCK_RX_BUF_SIZE];
static size_t g_rx_len = 0;
static size_t g_rx_pos = 0;
static uint8_t g_tx_buf[MOCK_TX_BUF_SIZE];
static size_t g_tx_len = 0;
static uint32_t g_clock_ms = 0;

int hal_uart_init(const hal_uart_config_t *config, hal_uart_handle_t **handle_out) {
    if (config == NULL || handle_out == NULL) {
        return -1;
    }
    g_handle.uart_num = config->uart_num;
    *handle_out = &g_handle;
    return 0;
}

int hal_uart_deinit(hal_uart_handle_t *handle) {
    if (handle == NULL) {
        return -1;
    }
    return 0;
}

int hal_uart_send(hal_uart_handle_t *handle, const uint8_t *data, size_t len) {
    if (handle == NULL || data == NULL) {
        return -1;
    }
    size_t space = sizeof(g_tx_buf) - g_tx_len;
    size_t to_copy = len < space ? len : space;
    memcpy(g_tx_buf + g_tx_len, data, to_copy);
    g_tx_len += to_copy;
    return (int)to_copy;
}

int hal_uart_receive(hal_uart_handle_t *handle, uint8_t *data, size_t max_len, uint32_t timeout_ms) {
    (void)timeout_ms;  // Mock is always non-blocking: returns whatever is buffered now.
    if (handle == NULL || data == NULL) {
        return -1;
    }
    size_t avail = g_rx_len - g_rx_pos;
    size_t to_copy = max_len < avail ? max_len : avail;
    memcpy(data, g_rx_buf + g_rx_pos, to_copy);
    g_rx_pos += to_copy;
    return (int)to_copy;
}

void mock_hal_uart_reset(void) {
    g_rx_len = 0;
    g_rx_pos = 0;
    g_tx_len = 0;
    g_clock_ms = 0;
}

void mock_hal_uart_push_rx(const uint8_t *data, size_t len) {
    // Compact already-consumed bytes out, then append the new bytes.
    size_t remaining = g_rx_len - g_rx_pos;
    memmove(g_rx_buf, g_rx_buf + g_rx_pos, remaining);
    g_rx_len = remaining;
    g_rx_pos = 0;

    size_t space = sizeof(g_rx_buf) - g_rx_len;
    size_t to_copy = len < space ? len : space;
    memcpy(g_rx_buf + g_rx_len, data, to_copy);
    g_rx_len += to_copy;
}

size_t mock_hal_uart_tx_len(void) {
    return g_tx_len;
}

const uint8_t *mock_hal_uart_tx_data(void) {
    return g_tx_buf;
}

void mock_hal_uart_clear_tx(void) {
    g_tx_len = 0;
}

void mock_clock_set_ms(uint32_t ms) {
    g_clock_ms = ms;
}

void mock_clock_advance_ms(uint32_t delta_ms) {
    g_clock_ms += delta_ms;
}

// Backs the freertos/task.h stub's xTaskGetTickCount() (portTICK_PERIOD_MS
// is 1 in the stub, so ticks and milliseconds are the same unit here).
uint32_t xTaskGetTickCount(void) {
    return g_clock_ms;
}
