/**
 * Minimal host-build stand-in for freertos/FreeRTOS.h.
 *
 * Only defines what auxbeam_light.c and command_queue.c actually use, so
 * the real production sources can be compiled and linked on a development
 * machine without pulling in a full FreeRTOS/ESP-IDF install (Constitution
 * Principle IV: Hardware-Abstracted Testing).
 */
#ifndef HOST_STUB_FREERTOS_H
#define HOST_STUB_FREERTOS_H

#include <stdint.h>

typedef uint32_t TickType_t;

#define portTICK_PERIOD_MS 1
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))

#endif /* HOST_STUB_FREERTOS_H */
