/**
 * @file command_queue.h
 * @brief Internal command queue interface
 */

#ifndef COMMAND_QUEUE_H
#define COMMAND_QUEUE_H

#include "auxbeam_light.h"
#include <stdbool.h>

// Full definition (not just a forward declaration) since auxbeam_light.c
// dereferences this type's members directly after command_queue_allocate()
// / command_queue_find() return a pointer to it.
typedef struct pending_command {
    uint8_t seq_id;
    auxbeam_output_state_t outputs[6];
    uint32_t send_time_ms;
    uint32_t timeout_ms;
    auxbeam_ack_callback_t ack_cb;
    auxbeam_timeout_callback_t timeout_cb;
    void *user_data;
    bool active;
} pending_command_t;

void command_queue_init(void);
pending_command_t* command_queue_allocate(uint8_t *seq_id_out);
pending_command_t* command_queue_find(uint8_t seq_id);
void command_queue_check_timeouts(void);
void command_queue_free(uint8_t seq_id);
void command_queue_cancel_all(void);
uint32_t command_queue_get_count(void);

#endif /* COMMAND_QUEUE_H */
