/**
 * @file command_queue.h
 * @brief Internal command queue interface
 */

#ifndef COMMAND_QUEUE_H
#define COMMAND_QUEUE_H

#include "auxbeam_light.h"

typedef struct pending_command pending_command_t;

void command_queue_init(void);
pending_command_t* command_queue_allocate(uint8_t *seq_id_out);
pending_command_t* command_queue_find(uint8_t seq_id);
void command_queue_check_timeouts(void);
void command_queue_free(uint8_t seq_id);
uint32_t command_queue_get_count(void);

#endif /* COMMAND_QUEUE_H */
