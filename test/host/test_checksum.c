/**
 * @file test_checksum.c
 * @brief Unit tests for light_protocol_calculate_checksum() (checksum.c).
 */
#include "light_protocol.h"
#include "test_util.h"
#include <stdint.h>

void run_checksum_tests(void) {
    // Single byte: sum is just that byte.
    uint8_t single[1] = {0x05};
    TEST_CHECK_EQ(light_protocol_calculate_checksum(single, 1), 0x05);

    // Zero-length input sums to zero.
    TEST_CHECK_EQ(light_protocol_calculate_checksum(single, 0), 0x00);

    // Sum below 256 needs no wrap.
    uint8_t no_wrap[3] = {0x01, 0x02, 0x03};
    TEST_CHECK_EQ(light_protocol_calculate_checksum(no_wrap, 3), 0x06);

    // 10 bytes of 0xFF = 2550, which must wrap modulo 256 (2550 % 256 = 246).
    uint8_t all_ff[10];
    for (int i = 0; i < 10; i++) {
        all_ff[i] = 0xFF;
    }
    TEST_CHECK_EQ(light_protocol_calculate_checksum(all_ff, 10), 246);

    // A real 10-byte command prefix (seq=0, cmd, output 0 ON, rest IGNORE)
    // matches the manually-computed sum: 0+0+8+12+0x81+0x88+0x88+0+0+0 = 421 % 256 = 165.
    uint8_t cmd_prefix[10] = {0x00, 0x00, 0x08, 0x0C, 0x81, 0x88, 0x88, 0x00, 0x00, 0x00};
    TEST_CHECK_EQ(light_protocol_calculate_checksum(cmd_prefix, 10), 165);
}
