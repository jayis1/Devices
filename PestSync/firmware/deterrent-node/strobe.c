/*
 * Deterrent Node — Strobe LED
 * firmware/deterrent-node/strobe.c
 *
 * High-intensity white strobe bursts for nocturnal pest aversion.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <stdlib.h>

#include "psp_protocol.h"

static const char *TAG = "STROBE";

#define PIN_STROBE  10

extern volatile uint8_t  deter_mode;
extern volatile bool     strobe_active;

void strobe_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Strobe task started");

    gpio_set_level(PIN_STROBE, 0);

    while (1) {
        if (deter_mode == DETER_OFF || deter_mode == DETER_SCHEDULE) {
            vTaskDelay(pdMS_TO_TICKS(30000));
            continue;
        }

        /* Strobe burst: 3× 100ms flashes, every 30 seconds during active mode */
        strobe_active = true;

        for (int i = 0; i < 3; i++) {
            gpio_set_level(PIN_STROBE, 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(PIN_STROBE, 0);
            vTaskDelay(pdMS_TO_TICKS(200));
        }

        strobe_active = false;

        /* Wait 30 sec between bursts (randomized to prevent habituation) */
        uint32_t wait = 25000 + (esp_random() % 10000);
        vTaskDelay(pdMS_TO_TICKS(wait));
    }
}