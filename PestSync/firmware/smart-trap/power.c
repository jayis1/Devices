/*
 * Smart Trap — Power Management (Ultra-Low Power)
 * firmware/smart-trap/power.c
 *
 * ESP32-C3 with 2× AA batteries. Event-driven: deep sleep between events.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "driver/gpio.h"

#include "psp_protocol.h"

static const char *TAG = "POWER";

void power_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Power management task started (2× AA, ultra-low power)");

    uint32_t uptime_s = 0;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(60000)); /* check every minute */
        uptime_s += 60;

        /* Estimate battery from ADC (2× AA = 3.0V nominal, 2.0V empty)
         * On ESP32-C3 with boost converter, we can read via ADC divider.
         * For now: simulate slow discharge. */
        uint8_t bat_pct;
        /* AA alkaline: ~2500 mAh at low drain (<1 mA avg)
         * Trap draws <10 µA in deep sleep, ~120 mA for 0.5s on TX
         * Average: ~8 µA → 2500/0.008 = 312,500 hours = 13 years theoretical
         * Realistic (self-discharge): 6-12 months */
        bat_pct = (uint8_t)(100 - (uptime_s / 157680) * 5); /* ~5% per 6 months */
        if (bat_pct > 100) bat_pct = 100;

        /* Update trap data battery level */
        extern trap_data_t g_trap_data;
        g_trap_data.battery_pct = bat_pct;

        if (bat_pct < 20) {
            g_trap_data.alerts |= ALERT_LOW_BATTERY;
            ESP_LOGW(TAG, "⚠️  Low battery: %d%%", bat_pct);
        }

        ESP_LOGD(TAG, "Battery: %d%% (uptime: %lu s)", bat_pct, (unsigned long)uptime_s);
    }
}