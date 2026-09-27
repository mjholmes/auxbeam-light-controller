/**
 * @file light_protocol.h
 * @brief Light controller protocol types and encoding/decoding functions
 *
 * Defines the wire protocol structures and functions for encoding commands
 * and decoding acknowledgments for the RA60 light controller.
 */

#ifndef LIGHT_PROTOCOL_H
#define LIGHT_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Light output state enumeration
 *
 * Maps to protocol nibble values per protocol.md specification.
 */
typedef enum {
    AUXBEAM_OUTPUT_OFF = 0,      /**< Output turned off */
    AUXBEAM_OUTPUT_ON = 1,       /**< Output turned on (steady) */
    AUXBEAM_OUTPUT_STROBE = 5,   /**< Output strobing */
    AUXBEAM_OUTPUT_IGNORE = 8    /**< Leave output in current state */
} auxbeam_output_state_t;

/**
 * @brief Protocol message structure (11-byte wire format)
 *
 * Represents both command and acknowledgment messages per protocol.md.
 */
typedef struct {
    uint8_t seq_id;              /**< Sequence ID (0x00-0xFF, wraps) */
    uint8_t msg_type;            /**< 0x00 = command, 0xFF = acknowledgment */
    uint8_t cmd_type;            /**< High nibble: 0x0/0xF, Low nibble: 0x8 */
    uint8_t unknown;             /**< 0x0C for output commands, 0x00 for ACKs */
    uint8_t outputs[4];          /**< 8 nibbles packed (6 used, 2 unused) */
    uint8_t padding[2];          /**< Always 0x00 */
    uint8_t checksum;            /**< Sum of bytes 0-9, modulo 256 */
} __attribute__((packed)) auxbeam_message_t;

/**
 * @brief Encode a command message
 *
 * Builds an 11-byte command message with the specified output states.
 *
 * @param[out] msg Pointer to message structure to fill
 * @param[in] seq_id Sequence ID for this command
 * @param[in] states Array of 6 output states
 */
void light_protocol_encode_command(auxbeam_message_t *msg, uint8_t seq_id, const auxbeam_output_state_t states[6]);

/**
 * @brief Decode an acknowledgment message
 *
 * Parses and validates an acknowledgment message.
 *
 * @param[in] msg Pointer to received message
 * @param[out] seq_id_out Pointer to store extracted sequence ID
 * @return true if valid ACK, false if invalid format
 */
bool light_protocol_decode_ack(const auxbeam_message_t *msg, uint8_t *seq_id_out);

/**
 * @brief Calculate checksum for message
 *
 * Computes 8-bit sum modulo 256 of first 10 bytes.
 *
 * @param[in] data Pointer to message data
 * @param[in] len Length of data (should be 10 for protocol messages)
 * @return Calculated checksum byte
 */
uint8_t light_protocol_calculate_checksum(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* LIGHT_PROTOCOL_H */
