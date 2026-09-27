/**
 * @file hal_uart_esp32.c
 * @brief ESP-IDF UART HAL implementation
 */

#include "hal_uart.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include <stdlib.h>
#include <string.h>

#define UART_BUF_SIZE 256

struct hal_uart_handle {
    int uart_num;
};

int hal_uart_init(const hal_uart_config_t *config, hal_uart_handle_t **handle_out) {
    if (config == NULL || handle_out == NULL) {
        return -1;
    }

    // Allocate handle
    hal_uart_handle_t *handle = (hal_uart_handle_t *)malloc(sizeof(hal_uart_handle_t));
    if (handle == NULL) {
        return -1;
    }
    handle->uart_num = config->uart_num;

    // Configure UART parameters
    uart_config_t uart_config = {
        .baud_rate = config->baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // Install UART driver
    if (uart_driver_install(config->uart_num, UART_BUF_SIZE, UART_BUF_SIZE, 0, NULL, 0) != ESP_OK) {
        free(handle);
        return -1;
    }

    if (uart_param_config(config->uart_num, &uart_config) != ESP_OK) {
        uart_driver_delete(config->uart_num);
        free(handle);
        return -1;
    }

    if (uart_set_pin(config->uart_num, config->tx_pin, config->rx_pin,
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
        uart_driver_delete(config->uart_num);
        free(handle);
        return -1;
    }

    *handle_out = handle;
    return 0;
}

int hal_uart_deinit(hal_uart_handle_t *handle) {
    if (handle == NULL) {
        return -1;
    }

    uart_driver_delete(handle->uart_num);
    free(handle);
    return 0;
}

int hal_uart_send(hal_uart_handle_t *handle, const uint8_t *data, size_t len) {
    if (handle == NULL || data == NULL) {
        return -1;
    }

    int sent = uart_write_bytes(handle->uart_num, (const char *)data, len);
    if (sent < 0) {
        return -1;
    }

    return sent;
}

int hal_uart_receive(hal_uart_handle_t *handle, uint8_t *data, size_t max_len, uint32_t timeout_ms) {
    if (handle == NULL || data == NULL) {
        return -1;
    }

    int received = uart_read_bytes(handle->uart_num, data, max_len,
                                   timeout_ms / portTICK_PERIOD_MS);
    if (received < 0) {
        return -1;
    }

    return received;
}
