/*
 * Pest Sentinel — PIR Motion Sensor
 * firmware/pest-sentinel/pir.c
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "psp_protocol.h"

static const char *TAG = "PIR";

#define PIN_PIR     17
#define PIN_IR_LED  41

extern bool g_ir_illumination_on;

/* Determine if it's "night" (should use IR illumination)
 * In production, this would use an LDR or time-of-day from RTC.
 * Here: always use IR for consistency in low-light placements. */
static bool is_low_light(void)
{
    /* Placeholder: check LDR or RTC hour.
     * Pest pathways are typically dark (under sinks, behind appliances).
     * Default: IR on. */
    return true;
}

void pir_task(void *pvParameters)
{
    ESP_LOGI(TAG, "PIR task started (AM312 on GPIO %d)", PIN_PIR);

    /* Configure PIR pin as interrupt */
    gpio_config_t io_cfg = {
        .pin_bit_mask = (1ULL << PIN_PIR),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
    };
    gpio_config(&io_cfg);

    /* Camera task handle for notification */
    TaskHandle_t camera_handle = NULL;

    while (1) {
        /* Wait for PIR to go high (motion detected) */
        int motion_count = 0;
        for (int i = 0; i < 5; i++) {
            if (gpio_get_level(PIN_PIR) == 1) {
                motion_count++;
                vTaskDelay(pdMS_TO_TICKS(10));
            } else {
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        }

        if (motion_count >= 3) {
            ESP_LOGI(TAG, " Motion detected!");

            /* Enable IR illumination if low light */
            g_ir_illumination_on = is_low_light();

            /* Notify camera task to capture */
            if (camera_handle == NULL)
                camera_handle = xTaskGetHandle("camera");
            if (camera_handle) {
                xTaskNotifyGive(camera_handle);
            }

            /* Debounce: wait before allowing next trigger */
            vTaskDelay(pdMS_TO_TICKS(2000));
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}