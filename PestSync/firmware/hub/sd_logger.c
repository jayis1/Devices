/*
 * Hub — microSD Logger
 * firmware/hub/sd_logger.c
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "psp_protocol.h"
#include "sensor_types.h"

static const char *TAG = "SD_LOG";

#define PIN_SD_MISO  19
#define PIN_SD_MOSI  23
#define PIN_SD_SCK   18
#define PIN_SD_CS    25

static FILE *log_file = NULL;

extern QueueHandle_t get_detection_queue(void);

typedef struct {
    uint8_t  msg_type;
    uint16_t src_id;
    uint8_t  payload[PSP_MAX_PAYLOAD];
    uint8_t  payload_len;
} psp_event_t;

static const char *alert_name(uint8_t alert_bits)
{
    static char buf[128];
    buf[0] = '\0';
    if (alert_bits & ALERT_PEST_DETECTED)   strcat(buf, "PEST ");
    if (alert_bits & ALERT_TRAP_TRIGGERED)  strcat(buf, "TRAP ");
    if (alert_bits & ALERT_TRAP_TAMPERED)   strcat(buf, "TAMPER ");
    if (alert_bits & ALERT_LOW_BATTERY)     strcat(buf, "LOWBAT ");
    if (alert_bits & ALERT_SENSOR_FAULT)    strcat(buf, "FAULT ");
    if (alert_bits & ALERT_BAIT_LOW)        strcat(buf, "BAIT ");
    if (alert_bits & ALERT_OIL_LOW)         strcat(buf, "OIL ");
    if (alert_bits & ALERT_TERMITE_SWARM)   strcat(buf, "TERMITE! ");
    if (buf[0] == '\0') strcpy(buf, "none");
    return buf;
}

void sd_logger_task(void *pvParameters)
{
    ESP_LOGI(TAG, "SD logger task starting...");

    /* SD card init via SPI */
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = HSPI_HOST;

    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_SD_MOSI,
        .miso_io_num = PIN_SD_MISO,
        .sclk_io_num = PIN_SD_SCK,
        .max_transfer_sz = 4096,
    };
    spi_bus_initialize(HSPI_HOST, &buscfg, SPI_DMA_CH_AUTO);

    sdspi_device_config_t devcfg = {
        .host_id = HSPI_HOST,
        .gpio_cs = PIN_SD_CS,
    };
    sdmmc_card_t *card;
    esp_err_t ret = sdspi_card_init(&host, &devcfg, &card);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SD card init failed: %s — local-only logging to RAM", esp_err_to_name(ret));
        /* Continue without SD — events still processed in memory */
    } else {
        ESP_LOGI(TAG, "SD card initialized: %s", card->cid.name);
    }

    QueueHandle_t q = get_detection_queue();
    psp_event_t evt;

    while (1) {
        if (xQueueReceive(q, &evt, portMAX_DELAY) == pdPASS) {
            /* Log event */
            char logline[256];

            if (evt.src_id >= NODE_ID_SENTINEL_BASE && evt.src_id < NODE_ID_TRAP_BASE) {
                sentinel_data_t *d = (sentinel_data_t *)evt.payload;
                snprintf(logline, sizeof(logline),
                    "[SENT] 0x%04X pest=%s conf=%d%% count=%d thermal=%.1f ir=%d bat=%d alerts=%s\n",
                    evt.src_id, pest_class_name(d->pest_class), d->confidence,
                    d->count_since_last, d->thermal_max_c / 10.0f, d->ir_illumination,
                    d->battery_pct, alert_name(d->alerts));
            } else if (evt.src_id >= NODE_ID_TRAP_BASE && evt.src_id < NODE_ID_DETERRENT_BASE) {
                trap_data_t *d = (trap_data_t *)evt.payload;
                snprintf(logline, sizeof(logline),
                    "[TRAP] 0x%04X status=%s weight=%dg bait=%d%% class=%d bat=%d alerts=%s\n",
                    evt.src_id, trap_status_name(d->trap_status), d->catch_weight_g,
                    d->bait_level, d->catch_class, d->battery_pct, alert_name(d->alerts));
            } else if (evt.src_id >= NODE_ID_DETERRENT_BASE) {
                deterrent_data_t *d = (deterrent_data_t *)evt.payload;
                snprintf(logline, sizeof(logline),
                    "[DETR] 0x%04X us=%d strobe=%d diff=%d oil=%d%% us_s=%lu doses=%d bat=%d alerts=%s\n",
                    evt.src_id, d->ultrasonic_active, d->strobe_active, d->diffuser_active,
                    d->oil_level, (unsigned long)d->total_ultrasonic_s, d->diffuser_doses,
                    d->battery_pct, alert_name(d->alerts));
            } else {
                snprintf(logline, sizeof(logline), "[????] 0x%04X type=0x%02X len=%d\n",
                    evt.src_id, evt.msg_type, evt.payload_len);
            }

            ESP_LOGI(TAG, "%s", logline);

            /* Write to SD if available */
            if (log_file) {
                fputs(logline, log_file);
                fflush(log_file);
            }
        }
    }
}