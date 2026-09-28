/**
 * @file light_protocol.c
 * @brief Protocol message encoding and decoding implementation
 */

#include "light_protocol.h"
#include <string.h>

void light_protocol_encode_command(auxbeam_message_t *msg, uint8_t seq_id, const auxbeam_output_state_t states[6]) {
    memset(msg, 0, sizeof(auxbeam_message_t));

    // Set fixed fields per protocol.md
    msg->seq_id = seq_id;
    msg->msg_type = 0x00;  // Command
    msg->cmd_type = 0x08;  // Output command
    msg->unknown = 0x0C;   // Output altering command marker

    // Pack 6 output states into 4 bytes, 2 nibbles per byte, lower-numbered
    // output in the HIGH nibble (captured from the RA60 panel: CH1 press is
    // 38 88 88 .., CH2 press is 83 88 88 ..).
    // outputs[0] = state 0 (high nibble) | state 1 (low nibble)
    // outputs[1] = state 2 (high nibble) | state 3 (low nibble)
    // outputs[2] = state 4 (high nibble) | state 5 (low nibble)
    // outputs[3] = outputs 7/8, not present on the RA60 (0x00, as the panel sends)
    msg->outputs[0] = ((states[0] & 0x0F) << 4) | (states[1] & 0x0F);
    msg->outputs[1] = ((states[2] & 0x0F) << 4) | (states[3] & 0x0F);
    msg->outputs[2] = ((states[4] & 0x0F) << 4) | (states[5] & 0x0F);
    msg->outputs[3] = 0x00;

    // Padding bytes always 0x00
    msg->padding[0] = 0x00;
    msg->padding[1] = 0x00;

    // Calculate checksum over first 10 bytes
    msg->checksum = light_protocol_calculate_checksum((const uint8_t *)msg, 10);
}

bool light_protocol_decode_ack(const auxbeam_message_t *msg, uint8_t *seq_id_out) {
    // Validate ACK format per protocol.md
    if (msg->msg_type != 0xFF) {
        return false;  // Not an acknowledgment
    }

    // Only the low nibble identifies the command class (0x8, output
    // command). The high nibble varies between units: 0xF is documented, a
    // real RA60 has been captured sending 0x1 (cmd_type 0x18). Its meaning
    // is unknown, so it is not checked.
    if ((msg->cmd_type & 0x0F) != 0x08) {
        return false;  // Invalid ACK cmd_type
    }

    // Verify checksum
    uint8_t calculated_checksum = light_protocol_calculate_checksum((const uint8_t *)msg, 10);
    if (calculated_checksum != msg->checksum) {
        return false;  // Checksum mismatch
    }

    // Extract sequence ID
    if (seq_id_out != NULL) {
        *seq_id_out = msg->seq_id;
    }

    return true;
}
