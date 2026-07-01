/**
 * MigraineSync — Hub Sub-GHz Driver (SX1262)
 * ==========================================
 * TDMA mesh coordinator for Sub-GHz 868 MHz communication
 * with Env Sentinel nodes.
 *
 * License: MIT
 */

#include "subghz.h"
#include "config.h"
#include "../common/protocol.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <esp_log.h>

static const char *TAG = "migrainesync_subghz";

static spi_device_handle_t s_spi;

/* ── SX1262 Registers (subset) ──────────────────────────── */
#define SX1262_REG_PKT_STATUS  0x00
#define SX1262_REG_SYNC_WORD   0x0740
#define SX1262_CMD_SET_TX      0x83
#define SX1262_CMD_SET_RX      0x82
#define SX1262_CMD_SET_STANDBY 0x80
#define SX1262_CMD_WRITE_BUF   0x0E
#define SX1262_CMD_READ_BUF    0x1E
#define SX1262_CMD_SET_RF_FREQ 0x86

/* ── SPI transfer ───────────────────────────────────────── */
static esp_err_t sx1262_spi_xfer(const uint8_t *tx, uint8_t *rx, size_t len)
{
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    return spi_device_polling_transmit(s_spi, &t);
}

/* ── Wait for BUSY pin low ──────────────────────────────── */
static void sx1262_wait_busy(void)
{
    while (gpio_get_level(SUBGHZ_BUSY_PIN) == 1)
        vTaskDelay(pdMS_TO_TICKS(1));
}

/* ── Write register ─────────────────────────────────────── */
static void sx1262_write_reg(uint16_t addr, uint8_t val)
{
    sx1262_wait_busy();
    uint8_t tx[4] = { 0x0D, (addr >> 8) & 0xFF, addr & 0xFF, val };
    gpio_set_level(SUBGHZ_TX_PIN, 0);
    sx1262_spi_xfer(tx, NULL, 4);
    gpio_set_level(SUBGHZ_TX_PIN, 1);
}

/* ── Set RF frequency ───────────────────────────────────── */
static void sx1262_set_freq(uint32_t freq_hz)
{
    /* freq = freq_hz * 2^25 / 32 MHz */
    uint64_t frac = ((uint64_t)freq_hz << 25) / 32000000ULL;
    uint8_t tx[5] = {
        SX1262_CMD_SET_RF_FREQ,
        (frac >> 24) & 0xFF,
        (frac >> 16) & 0xFF,
        (frac >> 8) & 0xFF,
        frac & 0xFF,
    };
    sx1262_wait_busy();
    gpio_set_level(SUBGHZ_TX_PIN, 0);
    sx1262_spi_xfer(tx, NULL, 5);
    gpio_set_level(SUBGHZ_TX_PIN, 1);
}

/* ── Send packet ────────────────────────────────────────── */
int subghz_send(const uint8_t *data, size_t len)
{
    if (len > 128)
        return -1;

    sx1262_wait_busy();

    /* Write data to TX buffer */
    uint8_t tx[3 + 128] = { SX1262_CMD_WRITE_BUF, 0x00 };
    tx[1] = 0;  /* offset */
    memcpy(&tx[2], data, len);
    gpio_set_level(SUBGHZ_TX_PIN, 0);
    sx1262_spi_xfer(tx, NULL, 2 + len);
    gpio_set_level(SUBGHZ_TX_PIN, 1);

    /* Set TX mode with timeout */
    sx1262_wait_busy();
    uint8_t tx_cmd[4] = { SX1262_CMD_SET_TX, 0x00, 0x01, 0x00 }; /* 256 ms timeout */
    gpio_set_level(SUBGHZ_TX_PIN, 0);
    sx1262_spi_xfer(tx_cmd, NULL, 4);
    gpio_set_level(SUBGHZ_TX_PIN, 1);

    ESP_LOGI(TAG, "TX %u bytes", (unsigned)len);
    return (int)len;
}

/* ── Receive packet (blocking with timeout) ─────────────── */
int subghz_recv(uint8_t *buf, size_t max_len, uint32_t timeout_ms)
{
    /* Set RX mode */
    sx1262_wait_busy();
    uint8_t rx_cmd[4] = { SX1262_CMD_SET_RX, 0x00, 0x00, 0x00 };
    /* timeout in 15.625 us units */
    uint32_t timeout_units = (timeout_ms * 1000) / 15625;
    rx_cmd[1] = (timeout_units >> 16) & 0xFF;
    rx_cmd[2] = (timeout_units >> 8) & 0xFF;
    rx_cmd[3] = timeout_units & 0xFF;
    gpio_set_level(SUBGHZ_TX_PIN, 0);
    sx1262_spi_xfer(rx_cmd, NULL, 4);
    gpio_set_level(SUBGHZ_TX_PIN, 1);

    /* Wait for DIO1 interrupt (packet received) */
    uint32_t waited = 0;
    while (gpio_get_level(SUBGHZ_DIO1_PIN) == 0) {
        if (waited >= timeout_ms)
            return 0;  /* timeout, no data */
        vTaskDelay(pdMS_TO_TICKS(10));
        waited += 10;
    }

    /* Read packet: first get status, then read buffer */
    sx1262_wait_busy();
    uint8_t read_cmd[3] = { SX1262_CMD_READ_BUF, 0x00, 0x00 };
    uint8_t rx_data[130] = {0};
    gpio_set_level(SUBGHZ_TX_PIN, 0);
    sx1262_spi_xfer(read_cmd, rx_data, 3 + max_len);
    gpio_set_level(SUBGHZ_TX_PIN, 1);

    size_t pkt_len = rx_data[2];  /* last status byte has length info */
    if (pkt_len > max_len)
        pkt_len = max_len;
    memcpy(buf, &rx_data[3], pkt_len);

    ESP_LOGI(TAG, "RX %u bytes", (unsigned)pkt_len);
    return (int)pkt_len;
}

/* ── Initialize SX1262 ──────────────────────────────────── */
int subghz_init(void)
{
    ESP_LOGI(TAG, "Initializing SX1262 on HSPI");

    /* Init CS pin */
    gpio_set_direction(SUBGHZ_TX_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(SUBGHZ_TX_PIN, 1);

    /* Init BUSY, DIO1 as input */
    gpio_set_direction(SUBGHZ_BUSY_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(SUBGHZ_DIO1_PIN, GPIO_MODE_INPUT);

    /* Init RESET */
    gpio_set_direction(SUBGHZ_RST_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(SUBGHZ_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(SUBGHZ_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Init HSPI */
    spi_bus_config_t buscfg = {
        .miso_io_num = SUBGHZ_MISO_PIN,
        .mosi_io_num = SUBGHZ_MOSI_PIN,
        .sclk_io_num = SUBGHZ_SCK_PIN,
        .max_transfer_sz = 130,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, 1);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 8000000,  /* 8 MHz */
        .mode = 0,
        .spics_io_num = -1,  /* manual CS control */
        .queue_size = 4,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &s_spi);

    /* Set frequency */
    sx1262_set_freq(SUBGHZ_FREQ_EU);

    /* Standby mode */
    sx1262_wait_busy();
    uint8_t standby[2] = { SX1262_CMD_SET_STANDBY, 0x01 }; /* RC 13 MHz */
    gpio_set_level(SUBGHZ_TX_PIN, 0);
    sx1262_spi_xfer(standby, NULL, 2);
    gpio_set_level(SUBGHZ_TX_PIN, 1);

    ESP_LOGI(TAG, "SX1262 initialized at 868.1 MHz, SF7, BW=125kHz");
    return 0;
}