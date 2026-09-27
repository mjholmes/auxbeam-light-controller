/**
 * @file command_queue.c
 * @brief Command queue implementation for tracking pending commands
 */

#include "command_queue.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <assert.h>
#include <string.h>

#define MAX_PENDING_COMMANDS 4

// T016: Pending command structure
struct pending_command {
    uint8_t seq_id;
    auxbeam_output_state_t outputs[6];
    uint32_t send_time_ms;
    uint32_t timeout_ms;
    auxbeam_ack_callback_t ack_cb;
    auxbeam_timeout_callback_t timeout_cb;
    void *user_data;
    bool active;
};

// T017: Command queue structure
typedef struct {
    struct pending_command slots[MAX_PENDING_COMMANDS];
    uint8_t next_seq_id;
    uint32_t pending_count;
} command_queue_t;

// Internal functions
static command_queue_t g_queue = {0};

// Debug-only invariant check: pending_count must always equal the number of
// active slots (data-model.md Global Invariants #2). Compiled out via
// assert() when NDEBUG is defined.
static uint32_t command_queue_count_active(void) {
    uint32_t n = 0;
    for (int i = 0; i < MAX_PENDING_COMMANDS; i++) {
        if (g_queue.slots[i].active) {
            n++;
        }
    }
    return n;
}

void command_queue_init(void) {
    memset(&g_queue, 0, sizeof(command_queue_t));
    g_queue.next_seq_id = 0;
    g_queue.pending_count = 0;
}

// T018: Allocate command slot
pending_command_t* command_queue_allocate(uint8_t *seq_id_out) {
    // Find first inactive slot
    for (int i = 0; i < MAX_PENDING_COMMANDS; i++) {
        if (!g_queue.slots[i].active) {
            pending_command_t *cmd = &g_queue.slots[i];
            memset(cmd, 0, sizeof(pending_command_t));
            cmd->active = true;
            cmd->seq_id = g_queue.next_seq_id;

            if (seq_id_out != NULL) {
                *seq_id_out = g_queue.next_seq_id;
            }

            // Increment sequence ID (wraps at 256)
            g_queue.next_seq_id++;
            g_queue.pending_count++;
            assert(g_queue.pending_count == command_queue_count_active());

            return cmd;
        }
    }

    return NULL;  // Queue full
}

// T019: Find command by sequence ID
pending_command_t* command_queue_find(uint8_t seq_id) {
    for (int i = 0; i < MAX_PENDING_COMMANDS; i++) {
        if (g_queue.slots[i].active && g_queue.slots[i].seq_id == seq_id) {
            return &g_queue.slots[i];
        }
    }
    return NULL;
}

// T020: Check timeouts and invoke callbacks
void command_queue_check_timeouts(void) {
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    for (int i = 0; i < MAX_PENDING_COMMANDS; i++) {
        pending_command_t *cmd = &g_queue.slots[i];

        if (cmd->active) {
            uint32_t elapsed = current_time - cmd->send_time_ms;

            if (elapsed >= cmd->timeout_ms) {
                // Timeout occurred
                if (cmd->timeout_cb != NULL) {
                    cmd->timeout_cb(cmd->seq_id, AUXBEAM_ERR_TIMEOUT, cmd->user_data);
                }

                // Free the slot
                cmd->active = false;
                g_queue.pending_count--;
            }
        }
    }

    assert(g_queue.pending_count == command_queue_count_active());
}

// T021: Free command slot
void command_queue_free(uint8_t seq_id) {
    pending_command_t *cmd = command_queue_find(seq_id);
    if (cmd != NULL && cmd->active) {
        cmd->active = false;
        g_queue.pending_count--;
    }
    assert(g_queue.pending_count == command_queue_count_active());
}

// Cancel all pending commands without invoking their callbacks (used by
// auxbeam_light_deinit(), per its documented contract).
void command_queue_cancel_all(void) {
    for (int i = 0; i < MAX_PENDING_COMMANDS; i++) {
        g_queue.slots[i].active = false;
    }
    g_queue.pending_count = 0;
    assert(g_queue.pending_count == command_queue_count_active());
}

uint32_t command_queue_get_count(void) {
    return g_queue.pending_count;
}
