/*
 * Hub — microSD Data Logger
 * firmware/hub/sd_logger.c
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/sdspi_host.h"
#include "driver/sdmmc_host.h"
#include "fatfs.h"
#include "csp_protocol.h"
#include "sensor_types.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *TAG = "SD_LOG";

extern QueueHandle_t telemetry_queue;

static FILE *log_file = NULL;
static int log_rotation_count = 0;

static void rotate_log(void)
{
    char filename[32];
    snprintf(filename, sizeof(filename),
             "/sdcard/compost_%04d.csv", log_rotation_count);

    if (log_file) fclose(log_file);
    log_file = fopen(filename, "w");
    if (log_file) {
        fprintf(log_file, "timestamp,node_id,uptime,batt,t1,t2,t3,m1,m2,m3,co2,ch4,mass,vent,phase,alerts\n");
        ESP_LOGI(TAG, "Opened log: %s", filename);
    }
    log_rotation_count++;
}

void sd_logger_task(void *pvParameters)
{
    /* Init SD card (SDSPI on SPI2 — but we share with LoRa, so use SDMMC or separate SPI).
     * For simplicity, this stub shows the logging logic. */
    ESP_LOGI(TAG, "SD logger starting...");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = 25;  /* SD CS pin */
    slot_config.host = SDSPI_DEFAULT_HOST;

    esp_err_t ret = sdspi_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SD card init failed (%s), logging disabled", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }

    /* Mount FAT */
    ret = mount_sdcard();  /* Uses fatfs to mount /sdcard */
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "FAT mount failed, logging disabled");
        vTaskDelete(NULL);
        return;
    }

    rotate_log();

    bin_node_data_t data;
    uint32_t lines_written = 0;

    while (1) {
        if (xQueueReceive(telemetry_queue, &data, portMAX_DELAY) == pdTRUE) {
            if (log_file) {
                /* Get timestamp (would use SNTP in production) */
                uint32_t ts = xTaskGetTickCount() * portTICK_PERIOD_MS;

                fprintf(log_file, "%lu,%04X,%lu,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s,%d\n",
                    (unsigned long)ts,
                    data.node_id,
                    (unsigned long)data.uptime_s,
                    data.battery_pct,
                    data.temp_c[0], data.temp_c[1], data.temp_c[2],
                    data.moisture_pct[0], data.moisture_pct[1], data.moisture_pct[2],
                    data.co2_ppm, data.methane_ppm,
                    data.mass_grams, data.vent_position,
                    csp_phase_name(data.phase), data.alerts);

                fflush(log_file);
                lines_written++;

                /* Rotate every 10000 lines */
                if (lines_written >= 10000) {
                    rotate_log();
                    lines_written = 0;
                }
            }
        }
    }
}