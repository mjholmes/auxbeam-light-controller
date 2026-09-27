/**
 * @file basic_example.c
 * @brief Basic usage example for AuxBeam light controller library
 *
 * auxbeam_light has no internal locking (see auxbeam_light.h's @warning):
 * every call, including auxbeam_light_process(), must come from the single
 * task that called auxbeam_light_init(). So sending commands and processing
 * ACKs/timeouts both happen in this one task's loop -- not a separate
 * "RX task" -- which is also where a real app would drain a FreeRTOS queue
 * of requests from other tasks (UI, CAN bus listener, etc.).
 */

#include "auxbeam_light.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"

static const char *TAG = "example";

// Acknowledgment callback
void on_ack(uint8_t seq_id, void *user_data) {
    ESP_LOGI(TAG, "Command %d acknowledged", seq_id);
}

// Timeout/error callback
void on_timeout(uint8_t seq_id, auxbeam_error_t error, void *user_data) {
    ESP_LOGW(TAG, "Command %d failed with error %d", seq_id, error);
}

// The single task that owns the library: it calls init() itself (so it
// becomes the one task the library will accept calls from), then sends
// demo commands and processes ACKs/timeouts in its own loop.
void light_controller_task(void *param) {
    auxbeam_config_t *config = (auxbeam_config_t *)param;

    auxbeam_context_t *ctx = NULL;
    auxbeam_error_t err = auxbeam_light_init(config, &ctx);
    if (err != AUXBEAM_OK) {
        ESP_LOGE(TAG, "Failed to initialize light controller: %d", err);
        return;
    }
    ESP_LOGI(TAG, "Light controller initialized successfully");

    while (1) {
        // Turn all outputs ON
        ESP_LOGI(TAG, "Setting all outputs ON");
        auxbeam_output_state_t all_on[6] = {
            AUXBEAM_OUTPUT_ON, AUXBEAM_OUTPUT_ON, AUXBEAM_OUTPUT_ON,
            AUXBEAM_OUTPUT_ON, AUXBEAM_OUTPUT_ON, AUXBEAM_OUTPUT_ON
        };
        auxbeam_light_set_multiple(ctx, all_on, on_ack, on_timeout, NULL);
        for (int i = 0; i < 200; i++) {  // ~2s, polling process() every 10ms
            auxbeam_light_process(ctx);
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        // Strobe pattern
        ESP_LOGI(TAG, "Strobe pattern");
        auxbeam_output_state_t strobe[6] = {
            AUXBEAM_OUTPUT_STROBE, AUXBEAM_OUTPUT_OFF, AUXBEAM_OUTPUT_STROBE,
            AUXBEAM_OUTPUT_OFF, AUXBEAM_OUTPUT_STROBE, AUXBEAM_OUTPUT_OFF
        };
        auxbeam_light_set_multiple(ctx, strobe, on_ack, on_timeout, NULL);
        for (int i = 0; i < 200; i++) {
            auxbeam_light_process(ctx);
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        // Turn all outputs OFF
        ESP_LOGI(TAG, "Setting all outputs OFF");
        auxbeam_output_state_t all_off[6] = {
            AUXBEAM_OUTPUT_OFF, AUXBEAM_OUTPUT_OFF, AUXBEAM_OUTPUT_OFF,
            AUXBEAM_OUTPUT_OFF, AUXBEAM_OUTPUT_OFF, AUXBEAM_OUTPUT_OFF
        };
        auxbeam_light_set_multiple(ctx, all_off, on_ack, on_timeout, NULL);
        for (int i = 0; i < 200; i++) {
            auxbeam_light_process(ctx);
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

// Config must outlive the task that reads it in its first line -- static
// storage is simplest here since there's exactly one instance.
static auxbeam_config_t g_config = {
    .uart_num = UART_NUM_1,
    .tx_pin = 17,  // GPIO 17 for TX
    .rx_pin = 16,  // GPIO 16 for RX
    .default_timeout_ms = 500
};

void app_main(void) {
    // Note: app_main itself never calls auxbeam_light_init() -- the task
    // that will make all the library calls must be the one that does, so
    // init() happens inside light_controller_task instead.
    xTaskCreate(light_controller_task, "light_ctrl", 2048, &g_config, 5, NULL);
}
