/**
 * @file test_protocol.c
 * @brief Unit tests for light_protocol_encode_command()/decode_ack()
 * (light_protocol.c), independent of the queue or any HAL.
 */
#include "light_protocol.h"
#include "test_util.h"
#include <string.h>

static void test_encode_command(void) {
    auxbeam_output_state_t states[6] = {
        AUXBEAM_OUTPUT_ON, AUXBEAM_OUTPUT_STROBE, AUXBEAM_OUTPUT_OFF,
        AUXBEAM_OUTPUT_IGNORE, AUXBEAM_OUTPUT_IGNORE, AUXBEAM_OUTPUT_IGNORE,
    };
    auxbeam_message_t msg;
    light_protocol_encode_command(&msg, 0x07, states);

    TEST_CHECK_EQ(msg.seq_id, 0x07);
    TEST_CHECK_EQ(msg.msg_type, 0x00);
    TEST_CHECK_EQ(msg.cmd_type, 0x08);
    TEST_CHECK_EQ(msg.unknown, 0x0C);

    // Nibble packing per protocol.md: states[0] in the low nibble, states[1]
    // in the high nibble of outputs[0], and so on.
    TEST_CHECK_EQ(msg.outputs[0], (AUXBEAM_OUTPUT_ON & 0x0F) | ((AUXBEAM_OUTPUT_STROBE & 0x0F) << 4));
    TEST_CHECK_EQ(msg.outputs[1], (AUXBEAM_OUTPUT_OFF & 0x0F) | ((AUXBEAM_OUTPUT_IGNORE & 0x0F) << 4));
    TEST_CHECK_EQ(msg.outputs[2], (AUXBEAM_OUTPUT_IGNORE & 0x0F) | ((AUXBEAM_OUTPUT_IGNORE & 0x0F) << 4));
    TEST_CHECK_EQ(msg.outputs[3], 0x00);
    TEST_CHECK_EQ(msg.padding[0], 0x00);
    TEST_CHECK_EQ(msg.padding[1], 0x00);

    uint8_t expected_checksum = light_protocol_calculate_checksum((const uint8_t *)&msg, 10);
    TEST_CHECK_EQ(msg.checksum, expected_checksum);
}

// Builds a valid ACK frame, matching the real encoder's checksum.
static void build_valid_ack(uint8_t seq_id, auxbeam_message_t *msg) {
    memset(msg, 0, sizeof(*msg));
    msg->seq_id = seq_id;
    msg->msg_type = 0xFF;
    msg->cmd_type = 0xF8;
    msg->checksum = light_protocol_calculate_checksum((const uint8_t *)msg, 10);
}

static void test_decode_ack_valid(void) {
    auxbeam_message_t msg;
    build_valid_ack(0x2A, &msg);

    uint8_t seq_id_out = 0;
    TEST_CHECK(light_protocol_decode_ack(&msg, &seq_id_out));
    TEST_CHECK_EQ(seq_id_out, 0x2A);
}

// Spec.md US3 AC2: a corrupted (bad-checksum) acknowledgment must be rejected.
static void test_decode_ack_bad_checksum(void) {
    auxbeam_message_t msg;
    build_valid_ack(0x01, &msg);
    msg.checksum ^= 0xFF;  // corrupt it

    uint8_t seq_id_out = 0xAA;
    TEST_CHECK(!light_protocol_decode_ack(&msg, &seq_id_out));
}

static void test_decode_ack_wrong_msg_type(void) {
    auxbeam_message_t msg;
    build_valid_ack(0x01, &msg);
    msg.msg_type = 0x00;  // looks like a command, not an ACK
    msg.checksum = light_protocol_calculate_checksum((const uint8_t *)&msg, 10);

    TEST_CHECK(!light_protocol_decode_ack(&msg, NULL));
}

static void test_decode_ack_wrong_cmd_type(void) {
    auxbeam_message_t msg;
    build_valid_ack(0x01, &msg);
    msg.cmd_type = 0xF7;  // valid ACK marker, wrong command class
    msg.checksum = light_protocol_calculate_checksum((const uint8_t *)&msg, 10);

    TEST_CHECK(!light_protocol_decode_ack(&msg, NULL));
}

void run_protocol_tests(void) {
    test_encode_command();
    test_decode_ack_valid();
    test_decode_ack_bad_checksum();
    test_decode_ack_wrong_msg_type();
    test_decode_ack_wrong_cmd_type();
}
