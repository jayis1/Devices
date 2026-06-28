/*
 * Deterrent Node — Frequency-Agile Ultrasonic Emission
 * firmware/deterrent-node/ultrasonic.c
 *
 * Sweeps 20-65 kHz in randomized patterns to prevent habituation.
 * Species-tuned: rodents 20-30 kHz, insects 40-60 kHz.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include <stdlib.h>
#include <math.h>

#include "psp_protocol.h"

static const char *TAG = "ULTRA";

#define PIN_ULTRASONIC  9
#define LEDC_CH         LEDC_CHANNEL_0
#define LEDC_TMR        LEDC_TIMER_0

extern volatile uint8_t  deter_mode;
extern volatile uint8_t  deter_band;
extern volatile uint16_t deter_duration_s;
extern volatile bool     ultrasonic_active;
extern volatile uint32_t total_ultrasonic_s;

/* Frequency range per band */
static const uint32_t band_min_hz[3] = { 20000, 40000, 20000 };  /* rodent, insect, both */
static const uint32_t band_max_hz[3] = { 30000, 60000, 65000 };  /* rodent, insect, both */

static void set_frequency(uint32_t freq_hz)
{
    ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TMR, freq_hz);
}

static uint32_t get_random_freq(void)
{
    uint32_t min_hz = band_min_hz[deter_band];
    uint32_t max_hz = band_max_hz[deter_band];
    return min_hz + (esp_random() % (max_hz - min_hz));
}

void ultrasonic_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Ultrasonic task started (frequency-agile, mode=%d band=%d)",
             deter_mode, deter_band);

    while (1) {
        if (deter_mode == DETER_OFF) {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CH, 0);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CH);
            ultrasonic_active = false;
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        if (deter_mode == DETER_ALWAYS_ON || deter_mode == DETER_ADAPTIVE) {
            /* Active emission cycle */
            ultrasonic_active = true;

            uint32_t elapsed = 0;
            while (elapsed < deter_duration_s) {
                /* Sweep frequency every 500 ms to prevent habituation */
                uint32_t freq = get_random_freq();
                set_frequency(freq);
                ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CH, 128); /* 50% duty */
                ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CH);

                ESP_LOGD(TAG, "Ultrasonic: %lu Hz", (unsigned long)freq);

                vTaskDelay(pdMS_TO_TICKS(500));
                elapsed++;

                /* In ADAPTIVE mode, add random silent gaps (2-5 sec off per minute) */
                if (deter_mode == DETER_ADAPTIVE && (esp_random() % 12) == 0) {
                    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CH, 0);
                    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CH);
                    uint32_t gap_ms = 2000 + (esp_random() % 3000);
                    vTaskDelay(pdMS_TO_TICKS(gap_ms));
                }
            }

            total_ultrasonic_s += elapsed;

            /* Rest period: 60 sec off after active cycle */
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CH, 0);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CH);
            ultrasonic_active = false;

            if (deter_mode == DETER_ALWAYS_ON) {
                vTaskDelay(pdMS_TO_TICKS(10000)); /* 10 sec rest in always-on */
            } else {
                vTaskDelay(pdMS_TO_TICKS(60000)); /* 60 sec rest in adaptive */
            }
        } else if (deter_mode == DETER_SCHEDULE) {
            /* In schedule mode, ultrasonic_task just waits —
             * scheduling is controlled by lora_node commands */
            ultrasonic_active = false;
            vTaskDelay(pdMS_TO_TICKS(10000));
        }
    }
}