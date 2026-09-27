/**
 * @file checksum.c
 * @brief Checksum calculation implementation
 */

#include "light_protocol.h"

uint8_t light_protocol_calculate_checksum(const uint8_t *data, size_t len) {
    uint32_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return (uint8_t)(sum % 256);
}
