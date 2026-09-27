/**
 * Minimal host-build stand-in for esp_log.h: prints to stdio instead of
 * the ESP-IDF logging subsystem.
 */
#ifndef HOST_STUB_ESP_LOG_H
#define HOST_STUB_ESP_LOG_H

#include <stdio.h>
#include <stddef.h>

#define ESP_LOGE(tag, fmt, ...) fprintf(stderr, "[E][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) fprintf(stderr, "[W][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) fprintf(stdout, "[I][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) fprintf(stdout, "[D][%s] " fmt "\n", tag, ##__VA_ARGS__)

#define ESP_LOG_DEBUG 0

static inline void esp_log_buffer_hex_host(const char *tag, const void *buf, size_t len) {
    fprintf(stdout, "[D][%s] ", tag);
    for (size_t i = 0; i < len; i++) {
        fprintf(stdout, "%02X ", ((const unsigned char *)buf)[i]);
    }
    fprintf(stdout, "\n");
}

#define ESP_LOG_BUFFER_HEX_LEVEL(tag, buf, len, level) esp_log_buffer_hex_host((tag), (buf), (len))

#endif /* HOST_STUB_ESP_LOG_H */
