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

// T017: Command queue structure (T016's pending_command_t is defined in
// command_queue.h -- see the comment there for why).
typedef struct {
    pending_command_t slots[MAX_PENDING_COMMANDS];
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
                // Capture what the callback needs before freeing the slot --
                // the callback may reentrantly call command_queue_allocate()
                // (via set_output()/set_multiple(), which is documented as
                // safe to do from here), and that could immediately reuse
                // this same slot's memory.
                uint8_t seq_id = cmd->seq_id;
                auxbeam_timeout_callback_t timeout_cb = cmd->timeout_cb;
                void *user_data = cmd->user_data;

                // Free the slot *before* invoking the callback, so a retry
                // from inside the callback doesn't see a stale QUEUE_FULL.
                cmd->active = false;
                g_queue.pending_count--;

                if (timeout_cb != NULL) {
                    timeout_cb(seq_id, AUXBEAM_ERR_TIMEOUT, user_data);
                }
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
