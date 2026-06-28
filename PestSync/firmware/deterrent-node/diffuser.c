/*
 * Deterrent Node — Essential Oil Diffuser
 * firmware/deterrent-node/diffuser.c
 *
 * Piezo ultrasonic atomizer for peppermint/cedar/eucalyptus oil.
 * Micro-doses on schedule. Refill alert when low.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_log.h"
#include <stdlib.h>

#include "psp_protocol.h"

static const char *TAG = "DIFFUSER";

#define PIN_DIFFUSER  1
#define PIN_OIL_ADC   0   /* ADC1_CH0 */

extern volatile bool     diffuser_active;
extern volatile uint16_t diffuser_doses;
extern deterrent_data_t  g_deterrent_data;

/* Each dose atomizes ~0.05 mL of oil for ~3 seconds */
#define DOSE_DURATION_MS  3000
#define DOSE_INTERVAL_S   3600  /* 1 hour between doses in adaptive mode */

static uint8_t read_oil_level(void)
{
    uint32_t raw = 0;
    for (int i = 0; i < 10; i++) {
        raw += adc1_get_raw(ADC1_CHANNEL_0);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    raw /= 10;

    /* Capacitive oil level sensor:
     * Full: ADC ~1500 (oil has high dielectric)
     * Empty: ADC ~4000 (air)
     * Map to 0-100% */
    int level = (int)((4000 - raw) * 100 / (4000 - 1500));
    if (level < 0) level = 0;
    if (level > 100) level = 100;
    return (uint8_t)level;
}

void diffuser_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Diffuser task started (piezo atomizer)");

    g_deterrent_data.oil_level = read_oil_level();
    ESP_LOGI(TAG, "Oil level: %d%%", g_deterrent_data.oil_level);

    while (1) {
        /* Check oil level every hour */
        g_deterrent_data.oil_level = read_oil_level();

        if (g_deterrent_data.oil_level < 20) {
            g_deterrent_data.alerts |= ALERT_OIL_LOW;
            ESP_LOGW(TAG, "⚠️  Oil low: %d%% — refill needed", g_deterrent_data.oil_level);
        } else {
            g_deterrent_data.alerts &= ~ALERT_OIL_LOW;
        }

        if (g_deterrent_data.oil_level < 5) {
            /* Empty — skip dosing */
            vTaskDelay(pdMS_TO_TICKS(DOSE_INTERVAL_S * 1000));
            continue;
        }

        /* Micro-dose: activate piezo atomizer for 3 seconds */
        diffuser_active = true;
        gpio_set_level(PIN_DIFFUSER, 1);
        ESP_LOGI(TAG, "Diffuser dose #%d (oil: %d%%)", diffuser_doses + 1,
                 g_deterrent_data.oil_level);

        vTaskDelay(pdMS_TO_TICKS(DOSE_DURATION_MS));

        gpio_set_level(PIN_DIFFUSER, 0);
        diffuser_active = false;
        diffuser_doses++;

        /* Wait for next dose interval */
        vTaskDelay(pdMS_TO_TICKS(DOSE_INTERVAL_S * 1000));
    }
}