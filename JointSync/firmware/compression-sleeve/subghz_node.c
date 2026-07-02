/**
 * JointSync Compression Sleeve — Sub-GHz Node
 *
 * CC1120 868 MHz, TDMA slave.
 *
 * License: MIT
 */

#include "subghz_node.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

static const char *TAG = "subghz_node";

#define CC1120_HOST    SPI2_HOST
#define CC1120_CS_PIN  1
#define CC1120_CLK_PIN 2
#define CC1120_MISO    3
#define CC1120_MOSI    4
#define CC1120_GPIO0   5

static spi_device_handle_t g_spi;
static subghz_data_cb_t g_callback = NULL;
static bool g_running = false;

/* Simplified CC1120 access (shared with hub implementation) */
static esp_err_t cc1120_strobe(uint8_t cmd)
{
    uint8_t tx[1] = {cmd};
    spi_transaction_t t = {0};
    t.length = 8;
    t.tx_buffer = tx;
    return spi_device_polling_transmit(g_spi, &t);
}

void subghz_node_init(subghz_data_cb_t callback)
{
    g_callback = callback;

    spi_bus_config_t buscfg = {
        .mosi_io_num = CC1120_MOSI,
        .miso_io_num = CC1120_MISO,
        .sclk_io_num = CC1120_CLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 256,
    };
    esp_err_t ret = spi_bus_initialize(CC1120_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return;
    }

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 4 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = CC1120_CS_PIN,
        .queue_size = 4,
    };
    spi_bus_add_device(CC1120_HOST, &devcfg, &g_spi);

    gpio_set_direction(CC1120_GPIO0, GPIO_MODE_INPUT);

    /* Software reset */
    cc1120_strobe(0x30);  /* SRES */
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "Sub-GHz node initialized (CC1120, 868 MHz)");
}

void subghz_node_start_rx(void)
{
    g_running = true;
    cc1120_strobe(0x34);  /* SRX */
    ESP_LOGI(TAG, "Sub-GHz RX started");
}

void subghz_node_stop(void)
{
    g_running = false;
    cc1120_strobe(0x36);  /* SIDLE */
}

void subghz_node_send(uint8_t *data, uint8_t len)
{
    if (!g_running || len > 128) return;

    /* Enter TX mode and send */
    /* In production: write to TX FIFO, strobe STX, wait for completion */
    cc1120_strobe(0x35);  /* STX */

    /* Wait for TX completion */
    int timeout = 100;
    while (gpio_get_level(CC1120_GPIO0) == 1 && timeout-- > 0) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    /* Return to RX */
    cc1120_strobe(0x34);  /* SRX */
}