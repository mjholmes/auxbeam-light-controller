/**
 * Minimal host-build stand-in for freertos/task.h.
 *
 * xTaskGetTickCount() is backed by the mock clock in mock_hal_uart.c so
 * tests can control elapsed time deterministically instead of relying on
 * a real OS tick.
 */
#ifndef HOST_STUB_FREERTOS_TASK_H
#define HOST_STUB_FREERTOS_TASK_H

#include "FreeRTOS.h"

typedef void *TaskHandle_t;

TickType_t xTaskGetTickCount(void);
TaskHandle_t xTaskGetCurrentTaskHandle(void);

#endif /* HOST_STUB_FREERTOS_TASK_H */
