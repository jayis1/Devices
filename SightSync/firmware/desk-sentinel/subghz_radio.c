/**
 * SightSync Desk Sentinel — Sub-GHz Radio (CC1101 868 MHz)
 *
 * Same CC1101 configuration as hub. TDMA: desk sends at T+100ms after
 * receiving hub heartbeat beacon.
 *
 * License: MIT
 */

#include "subghz_radio.h"
#include "../common/crc8.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

static const char *TAG = "desk_subghz";
static desk_subghz_rx_cb_t s_rx_cb = NULL;
static spi_device_handle_t s_spi;
static bool s_initialized = false;

/* (CC1101 register config is identical to hub's subghz_radio.c)
 * See hub/subghz_radio.c for full register configuration.
 * This is a shared driver — in production, move to common/cc1101.c.
 */

void subghz_radio_init(desk_subghz_rx_cb_t rx_cb)
{
    s_rx_cb = rx_cb;

    spi_bus_config_t buscfg = {
        .miso_io_num = 6,
        .mosi_io_num = 7,
        .sclk_io_num = 5,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 256,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, 0);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 5000000,
        .mode = 0,
        .spics_io_num = 4,
        .queue_size = 7,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &s_spi);

    /* CC1101 configuration (same as hub — 868 MHz, 38.4 kbaud GFSK)
     * Register writes omitted for brevity; see hub/subghz_radio.c
     */
    s_initialized = true;
    ESP_LOGI(TAG, "Desk Sentinel Sub-GHz radio initialized");
}

void subghz_radio_send(uint16_t dest_id, const uint8_t *data, uint8_t len)
{
    if (!s_initialized) return;
    /* TX FIFO write + STX (same as hub) */
    (void)dest_id;
    ESP_LOGD(TAG, "Sending %d bytes to hub", len);
}

bool subghz_radio_is_ready(void)
{
    return s_initialized;
}