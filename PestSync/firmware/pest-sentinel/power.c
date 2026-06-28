/*
 * Pest Sentinel — Power Management
 * firmware/pest-sentinel/power.c
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include <math.h>

#include "psp_protocol.h"

static const char *TAG = "POWER";

#define BATTERY_ADC_CHANNEL  ADC1_CHANNEL_0  /* GPIO1 on ESP32-S3 */
#define BATTERY_DIVIDER_R1  100000.0f  /* 100k */
#define BATTERY_DIVIDER_R2  100000.0f  /* 100k (gives 1:2 divider → 3.3V max for 6.6V) */

static esp_adc_cal_characteristics_t adc_chars;

uint8_t read_battery_pct_esp32(void)
{
    uint32_t raw = esp_adc_cal_get_voltage(BATTERY_ADC_CHANNEL, &adc_chars);
    /* Voltage at ADC = Vbat * R2 / (R1 + R2) = Vbat / 2 */
    float vbat = raw * 2.0f / 1000.0f; /* mV → V, with divider */

    /* 18650 Li-ion: 4.2V full, 3.0V empty, nominal 3.7V */
    float pct = (vbat - 3.0f) / (4.2f - 3.0f) * 100.0f;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return (uint8_t)pct;
}

void power_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Power management task started");

    /* ADC characterization */
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11,
                              ADC_WIDTH_BIT_12, 1100, &adc_chars);

    uint32_t uptime_s = 0;
    bool low_battery_alerted = false;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(60000)); /* check every minute */
        uptime_s += 60;

        uint8_t bat = read_battery_pct_esp32();

        if (bat < 20 && !low_battery_alerted) {
            ESP_LOGW(TAG, "⚠️  Low battery: %d%%", bat);
            low_battery_alerted = true;
        } else if (bat > 30) {
            low_battery_alerted = false;
        }

        ESP_LOGD(TAG, "Battery: %d%% (uptime: %lu s)", bat, (unsigned long)uptime_s);
    }
}