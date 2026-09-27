/**
 * @file quickstart.c
 * @brief Quickstart example matching specs/001-light-controller/quickstart.md
 *
 * Unlike basic_example.c (which just cycles through output patterns), this
 * demonstrates the "Common Patterns" from the quickstart guide: checking
 * queue capacity before sending, and retrying a command on timeout.
 *
 * auxbeam_light has no internal locking (see auxbeam_light.h's @warning):
 * every call, including auxbeam_light_process(), must come from the single
 * task that called auxbeam_light_init(). So everything here -- init,
 * sending, waiting for queue room, and processing ACKs/timeouts -- runs in
 * one task's loop. In particular, send_when_queue_has_room()'s wait loop
 * calls auxbeam_light_process() itself: nothing else will ever drain the
 * queue while this task is busy waiting.
 */

#include "auxbeam_light.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <stdlib.h>

static const char *TAG = "quickstart";
static auxbeam_context_t *g_light_ctx = NULL;

#define MAX_RETRIES 3

typedef struct {
    uint8_t output_index;
    auxbeam_output_state_t state;
    int retry_count;
} command_context_t;

static void on_ack(uint8_t seq_id, void *user_data) {
    ESP_LOGI(TAG, "Command %d acknowledged", seq_id);
    free(user_data);  // matches the retry_cmd_ctx allocated in send_with_retry()
}

// Pattern 1 (quickstart.md "Retry on Timeout"): retry up to MAX_RETRIES times
// before giving up on a command.
static void on_timeout_retry(uint8_t seq_id, auxbeam_error_t error, void *user_data) {
    command_context_t *cmd_ctx = (command_context_t *)user_data;
    ESP_LOGW(TAG, "Command %d failed: error %d", seq_id, error);

    if (cmd_ctx->retry_count < MAX_RETRIES) {
        cmd_ctx->retry_count++;
        ESP_LOGI(TAG, "Retry %d/%d for output %d", cmd_ctx->retry_count, MAX_RETRIES, cmd_ctx->output_index);
        auxbeam_light_set_output(g_light_ctx, cmd_ctx->output_index, cmd_ctx->state,
                                  on_ack, on_timeout_retry, cmd_ctx);
    } else {
        ESP_LOGE(TAG, "Command for output %d failed after %d retries", cmd_ctx->output_index, MAX_RETRIES);
        free(cmd_ctx);
    }
}

static void send_with_retry(uint8_t output_index, auxbeam_output_state_t state) {
    command_context_t *cmd_ctx = malloc(sizeof(command_context_t));
    if (cmd_ctx == NULL) {
        ESP_LOGE(TAG, "Out of memory queuing command");
        return;
    }
    cmd_ctx->output_index = output_index;
    cmd_ctx->state = state;
    cmd_ctx->retry_count = 0;
    auxbeam_light_set_output(g_light_ctx, output_index, state, on_ack, on_timeout_retry, cmd_ctx);
}

// Pattern 2 (quickstart.md "Check Queue Before Sending"): back off instead of
// hitting AUXBEAM_ERR_QUEUE_FULL when commands are already in flight. Must
// keep calling process() while waiting, since this task is the only thing
// that will ever drain the queue.
static void send_when_queue_has_room(uint8_t output_index, auxbeam_output_state_t state) {
    uint32_t pending = 0;
    auxbeam_light_get_pending_count(g_light_ctx, &pending);

    while (pending >= AUXBEAM_MAX_PENDING_COMMANDS) {
        ESP_LOGW(TAG, "Queue full (%u pending), waiting...", pending);
        auxbeam_light_process(g_light_ctx);
        vTaskDelay(pdMS_TO_TICKS(10));
        auxbeam_light_get_pending_count(g_light_ctx, &pending);
    }

    send_with_retry(output_index, state);
}

// Runs auxbeam_light_process() every ~10ms for the given duration, so ACKs/
// timeouts keep getting handled while the demo "waits" between phases.
static void process_for(uint32_t duration_ms) {
    for (uint32_t elapsed = 0; elapsed < duration_ms; elapsed += 10) {
        auxbeam_light_process(g_light_ctx);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// The single task that owns the library: it calls init() itself, then
// sends demo commands and processes ACKs/timeouts in its own loop.
static void light_controller_task(void *param) {
    auxbeam_config_t *config = (auxbeam_config_t *)param;

    auxbeam_error_t err = auxbeam_light_init(config, &g_light_ctx);
    if (err != AUXBEAM_OK) {
        ESP_LOGE(TAG, "Init failed: %d", err);
        return;
    }
    ESP_LOGI(TAG, "Light controller initialized");

    while (1) {
        for (uint8_t output = 0; output < AUXBEAM_NUM_OUTPUTS; output++) {
            send_when_queue_has_room(output, AUXBEAM_OUTPUT_ON);
        }
        process_for(2000);

        for (uint8_t output = 0; output < AUXBEAM_NUM_OUTPUTS; output++) {
            send_when_queue_has_room(output, AUXBEAM_OUTPUT_OFF);
        }
        process_for(2000);
    }
}

static auxbeam_config_t g_config = {
    .uart_num = UART_NUM_1,
    .tx_pin = 17,  // GPIO 17
    .rx_pin = 16,  // GPIO 16
    .default_timeout_ms = 500
};

void app_main(void) {
    // Note: app_main itself never calls auxbeam_light_init() -- the task
    // that will make all the library calls must be the one that does, so
    // init() happens inside light_controller_task instead.
    xTaskCreate(light_controller_task, "light_ctrl", 2048, &g_config, 5, NULL);
}
