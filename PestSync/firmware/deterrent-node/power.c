/*
 * Deterrent Node — Power Management
 * firmware/deterrent-node/power.c
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#include "psp_protocol.h"

static const char *TAG = "POWER";

extern deterrent_data_t g_deterrent_data;

static esp_adc_cal_characteristics_t adc_chars;

uint8_t read_battery_pct_esp32c3(void)
{
    /* ESP32-C3 ADC1_CH1 for battery via divider (adjust per HW) */
    uint32_t voltage = esp_adc_cal_get_voltage(ADC1_CHANNEL_1, &adc_chars);
    float vbat = voltage * 2.0f / 1000.0f; /* with 1:2 divider */

    float pct = (vbat - 3.0f) / (4.2f - 3.0f) * 100.0f;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return (uint8_t)pct;
}

void power_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Power management task started");

    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11,
                              ADC_WIDTH_BIT_12, 1100, &adc_chars);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(60000));

        uint8_t bat = read_battery_pct_esp32c3();
        g_deterrent_data.battery_pct = bat;

        if (bat < 20) {
            g_deterrent_data.alerts |= ALERT_LOW_BATTERY;
            ESP_LOGW(TAG, "⚠️  Low battery: %d%%", bat);
        } else {
            g_deterrent_data.alerts &= ~ALERT_LOW_BATTERY;
        }

        ESP_LOGD(TAG, "Battery: %d%%", bat);
    }
}