/**
 * @file mock_hal_uart.h
 * @brief Test-control API for the host mock of hal_uart.h.
 *
 * Lets host tests simulate the controller side of the link: push bytes
 * into the RX path (in whatever chunk sizes a test wants, to exercise
 * partial-frame arrival) and inspect what auxbeam_light sent via TX.
 */
#ifndef MOCK_HAL_UART_H
#define MOCK_HAL_UART_H

#include <stdint.h>
#include <stddef.h>

/** Reset all mock state (RX/TX buffers and the mock clock) to empty/zero. */
void mock_hal_uart_reset(void);

/** Simulate `len` bytes arriving on the wire from the controller. */
void mock_hal_uart_push_rx(const uint8_t *data, size_t len);

/** Bytes captured from the last (and all prior, since last clear) sends. */
size_t mock_hal_uart_tx_len(void);
const uint8_t *mock_hal_uart_tx_data(void);
void mock_hal_uart_clear_tx(void);

/** Mock clock backing xTaskGetTickCount(), so timeout logic is testable
 *  without real elapsed time. */
void mock_clock_set_ms(uint32_t ms);
void mock_clock_advance_ms(uint32_t delta_ms);

/** Mock "current task" backing xTaskGetCurrentTaskHandle(), so the
 *  owner-task guard is testable without real FreeRTOS tasks. Defaults to
 *  a fixed non-NULL sentinel; call with a different value to simulate a
 *  call arriving from a different task. */
void mock_task_set_current(void *handle);

#endif /* MOCK_HAL_UART_H */
