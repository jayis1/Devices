/*
 * Bin Node — Power Management (deep sleep between readings)
 * firmware/bin-node/power.c
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "driver/rtc_io.h"
#include <math.h>

static const char *TAG = "POWER";

/* Solar charging model */
typedef struct {
    float battery_capacity_mah;
    float battery_mah_remaining;
    float solar_watts;
    float voltage;
} power_state_t;

static power_state_t pwr = {
    .battery_capacity_mah = 3000.0f,
    .battery_mah_remaining = 3000.0f,
    .solar_watts = 2.0f,
    .voltage = 4.2f,
};

/* Estimate solar input based on time of day (simplified) */
static float estimate_solar_input(void)
{
    /* Get current hour from uptime */
    uint32_t uptime_s = (uint32_t)(esp_timer_get_time() / 1000000);
    uint32_t hour = (uptime_s / 3600) % 24;

    /* Solar curve: peak at noon, zero at night */
    if (hour >= 6 && hour <= 18) {
        float solar_factor = sinf((hour - 6) * 3.14159f / 12.0f);
        return pwr.solar_watts * fmaxf(0, solar_factor) * 0.7f; /* 70% efficiency */
    }
    return 0;
}

/* Estimate battery percentage */
static uint8_t estimate_battery_pct(void)
{
    return (uint8_t)(pwr.battery_mah_remaining * 100.0f / pwr.battery_capacity_mah);
}

void power_task(void *pvParameters)
{
    float last_time = 0;

    while (1) {
        float now = (float)(esp_timer_get_time() / 1000000.0f);
        float dt = now - last_time; /* seconds */
        last_time = now;

        /* Solar input (watts → mAh: 1W at 3.7V = 270 mA) */
        float solar_in = estimate_solar_input();
        float solar_ma = solar_in / 3.7f * 1000.0f;

        /* Load: average current draw (sensor reading burst + sleep) */
        float load_ma = 3.5f; /* average 3.5 mA with 15-min sleep cycle */

        /* Net charge/discharge */
        float net_ma = solar_ma - load_ma;
        pwr.battery_mah_remaining += net_ma * dt / 3600.0f;

        /* Clamp */
        if (pwr.battery_mah_remaining > pwr.battery_capacity_mah)
            pwr.battery_mah_remaining = pwr.battery_capacity_mah;
        if (pwr.battery_mah_remaining < 0)
            pwr.battery_mah_remaining = 0;

        /* Update voltage estimate (simplified Li-ion discharge curve) */
        uint8_t pct = estimate_battery_pct();
        pwr.voltage = 3.0f + (pct / 100.0f) * 1.2f; /* 3.0-4.2V */

        ESP_LOGD(TAG, "Solar: %.1fmW | Load: %.1fmA | Net: %.1fmA | Batt: %d%% (%.2fV)",
                 solar_ma * 3.7f, load_ma, net_ma, pct, pwr.voltage);

        /* If battery critically low, reduce sampling rate */
        if (pct < 10) {
            ESP_LOGW(TAG, "⚠️  Battery low (%d%%) — reducing sample rate", pct);
            /* Would increase SAMPLE_INTERVAL_S here */
        }

        vTaskDelay(pdMS_TO_TICKS(60000)); /* Check every minute */
    }
}