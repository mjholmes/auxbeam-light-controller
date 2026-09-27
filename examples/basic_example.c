/**
 * @file basic_example.c
 * @brief Basic usage example for AuxBeam light controller library
 */

#include "auxbeam_light.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"

static const char *TAG = "example";
static auxbeam_context_t *g_light_ctx = NULL;

// Acknowledgment callback
void on_ack(uint8_t seq_id, void *user_data) {
    ESP_LOGI(TAG, "Command %d acknowledged", seq_id);
}

// Timeout/error callback
void on_timeout(uint8_t seq_id, auxbeam_error_t error, void *user_data) {
    ESP_LOGW(TAG, "Command %d failed with error %d", seq_id, error);
}

// Light RX task - processes acknowledgments
void light_rx_task(void *param) {
    auxbeam_context_t *ctx = (auxbeam_context_t *)param;
    while (1) {
        auxbeam_light_process(ctx);
        vTaskDelay(pdMS_TO_TICKS(10));  // 10ms polling interval
    }
}

// Demo task - sends commands
void demo_task(void *param) {
    auxbeam_context_t *ctx = (auxbeam_context_t *)param;

    vTaskDelay(pdMS_TO_TICKS(1000));  // Wait 1 second for initialization

    while (1) {
        // Turn all outputs ON
        ESP_LOGI(TAG, "Setting all outputs ON");
        auxbeam_output_state_t all_on[6] = {
            AUXBEAM_OUTPUT_ON, AUXBEAM_OUTPUT_ON, AUXBEAM_OUTPUT_ON,
            AUXBEAM_OUTPUT_ON, AUXBEAM_OUTPUT_ON, AUXBEAM_OUTPUT_ON
        };
        auxbeam_light_set_multiple(ctx, all_on, on_ack, on_timeout, NULL);
        vTaskDelay(pdMS_TO_TICKS(2000));

        // Strobe pattern
        ESP_LOGI(TAG, "Strobe pattern");
        auxbeam_output_state_t strobe[6] = {
            AUXBEAM_OUTPUT_STROBE, AUXBEAM_OUTPUT_OFF, AUXBEAM_OUTPUT_STROBE,
            AUXBEAM_OUTPUT_OFF, AUXBEAM_OUTPUT_STROBE, AUXBEAM_OUTPUT_OFF
        };
        auxbeam_light_set_multiple(ctx, strobe, on_ack, on_timeout, NULL);
        vTaskDelay(pdMS_TO_TICKS(2000));

        // Turn all outputs OFF
        ESP_LOGI(TAG, "Setting all outputs OFF");
        auxbeam_output_state_t all_off[6] = {
            AUXBEAM_OUTPUT_OFF, AUXBEAM_OUTPUT_OFF, AUXBEAM_OUTPUT_OFF,
            AUXBEAM_OUTPUT_OFF, AUXBEAM_OUTPUT_OFF, AUXBEAM_OUTPUT_OFF
        };
        auxbeam_light_set_multiple(ctx, all_off, on_ack, on_timeout, NULL);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void app_main(void) {
    // Initialize light controller library
    auxbeam_config_t config = {
        .uart_num = UART_NUM_1,
        .tx_pin = 17,  // GPIO 17 for TX
        .rx_pin = 16,  // GPIO 16 for RX
        .default_timeout_ms = 500
    };

    auxbeam_error_t err = auxbeam_light_init(&config, &g_light_ctx);
    if (err != AUXBEAM_OK) {
        ESP_LOGE(TAG, "Failed to initialize light controller: %d", err);
        return;
    }

    ESP_LOGI(TAG, "Light controller initialized successfully");

    // Create RX task for processing acknowledgments
    xTaskCreate(light_rx_task, "light_rx", 2048, g_light_ctx, 5, NULL);

    // Create demo task for sending commands
    xTaskCreate(demo_task, "demo", 2048, g_light_ctx, 5, NULL);
}
