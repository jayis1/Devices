/*
 * cc1101.c — CC1101 Sub-GHz transceiver driver (868 MHz)
 *
 * SPI implementation for ESP32 (ESP-IDF). RP2040 port uses
 * pico-sdk SPI; the interface is identical.
 *
 * License: MIT
 */
#include "cc1101.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "CC1101";

/* ── Low-level SPI helpers ──────────────────────────────────── */

static esp_err_t cc1101_write_reg(cc1101_t *cc, uint8_t addr, uint8_t val)
{
    spi_transaction_t t = {0};
    uint8_t tx[2] = {addr & 0x3F, val};  /* write bit = 0 */
    t.length = 16;
    t.tx_buffer = tx;
    t.rx_buffer = NULL;
    return spi_device_polling_transmit(cc->spi, &t);
}

static esp_err_t cc1101_write_burst(cc1101_t *cc, uint8_t addr,
                                     const uint8_t *data, uint8_t len)
{
    spi_transaction_t t = {0};
    uint8_t tx[66];
    tx[0] = (addr & 0x3F) | CC1101_BURST_BIT;
    memcpy(&tx[1], data, len);
    t.length = (1 + len) * 8;
    t.tx_buffer = tx;
    return spi_device_polling_transmit(cc->spi, &t);
}

static uint8_t cc1101_read_reg(cc1101_t *cc, uint8_t addr)
{
    spi_transaction_t t = {0};
    uint8_t tx[2] = {addr | CC1101_READ_BIT, 0x00};
    uint8_t rx[2] = {0};
    t.length = 16;
    t.tx_buffer = tx;
    t.rx_buffer = rx;
    spi_device_polling_transmit(cc->spi, &t);
    return rx[1];
}

static uint8_t cc1101_read_status(cc1101_t *cc, uint8_t addr)
{
    spi_transaction_t t = {0};
    uint8_t tx[2] = {addr | CC1101_READ_BIT | CC1101_BURST_BIT, 0x00};
    uint8_t rx[2] = {0};
    t.length = 16;
    t.tx_buffer = tx;
    t.rx_buffer = rx;
    spi_device_polling_transmit(cc->spi, &t);
    return rx[1];
}

static esp_err_t cc1101_strobe(cc1101_t *cc, uint8_t strobe)
{
    spi_transaction_t t = {0};
    uint8_t tx = strobe;
    t.length = 8;
    t.tx_buffer = &tx;
    return spi_device_polling_transmit(cc->spi, &t);
}

static void cc1101_reset(cc1101_t *cc)
{
    /* Hardware reset via CSn toggle */
    gpio_set_level(cc->cs_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(1));
    gpio_set_level(cc->cs_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(1));
    gpio_set_level(cc->cs_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(45));
    /* SRES strobe */
    cc1101_strobe(cc, CC1101_SRES);
    vTaskDelay(pdMS_TO_TICKS(100));
}

/* ── Initialization ─────────────────────────────────────────── */

int cc1101_init(cc1101_t *cc, spi_host_device_t host,
                gpio_num_t cs, gpio_num_t gd0, gpio_num_t gd2,
                uint8_t addr)
{
    esp_err_t ret;

    /* Configure SPI device */
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 4 * 1000 * 1000,  /* 4 MHz */
        .mode = 0,                           /* CPOL=0, CPHA=0 */
        .spics_io_num = -1,                  /* manual CS control */
        .queue_size = 7,
        .flags = SPI_DEVICE_HALFDUPLEX,
    };
    ret = spi_bus_add_device(host, &devcfg, &cc->spi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_add_device failed: %s", esp_err_to_name(ret));
        return -1;
    }

    cc->cs_pin = cs;
    cc->gd0_pin = gd0;
    cc->gd2_pin = gd2;
    cc->my_addr = addr;

    /* Configure GPIO */
    gpio_set_direction(cs, GPIO_MODE_OUTPUT);
    gpio_set_level(cs, 1);

    if (gd0 >= 0) {
        gpio_set_direction(gd0, GPIO_MODE_INPUT);
        gpio_set_intr_type(gd0, GPIO_INTR_NEGEDGE);  /* falling edge = packet */
    }
    if (gd2 >= 0) {
        gpio_set_direction(gd2, GPIO_MODE_INPUT);
    }

    /* Hardware reset */
    cc1101_reset(cc);

    /* Register configuration for 868 MHz, 38.4 kBaud, GFSK */
    /* IOCFG0: GD0 = packet received interrupt (active low) */
    cc1101_write_reg(cc, CC1101_IOCFG0, 0x06);
    /* IOCFG2: GD2 = serial clock (unused, default) */
    cc1101_write_reg(cc, CC1101_IOCFG2, 0x0B);

    /* Sync word: 0x2DD4 (matches QuakeGuard protocol) */
    cc1101_write_reg(cc, CC1101_SYNC1, 0x2D);
    cc1101_write_reg(cc, CC1101_SYNC0, 0xD4);

    /* Packet: variable length, address check + CRC */
    cc1101_write_reg(cc, CC1101_PKTCTRL0, 0x45);  /* CRC on, var len, Manchester off */
    cc1101_write_reg(cc, CC1101_PKTCTRL1, 0x06);  /* address check, append status */
    cc1101_write_reg(cc, CC1101_ADDR, addr);       /* this node's address */
    cc1101_write_reg(cc, CC1101_PKTLEN, 0xFF);     /* max packet length */

    /* Frequency: 868.0 MHz */
    cc1101_write_reg(cc, CC1101_FREQ2, CC1101_FREQ2_VAL);
    cc1101_write_reg(cc, CC1101_FREQ1, CC1101_FREQ1_VAL);
    cc1101_write_reg(cc, CC1101_FREQ0, CC1101_FREQ0_VAL);

    /* Modem: 38.4 kBaud, GFSK, channel spacing 200 kHz */
    cc1101_write_reg(cc, CC1101_MDMCFG4, 0xCA);  /* DRATE_E=10, CHANBW=101kHz */
    cc1101_write_reg(cc, CC1101_MDMCFG3, 0x83);  /* DRATE_M=131 */
    cc1101_write_reg(cc, CC1101_MDMCFG2, 0x13);  /* GFSK, 30/32 sync bits */
    cc1101_write_reg(cc, CC1101_MDMCFG1, 0x22);  /* FEC off, 2 preamble bytes */
    cc1101_write_reg(cc, CC1101_MDMCFG0, 0xF8);  /* channel spacing */
    cc1101_write_reg(cc, CC1101_DEVIATN, 0x35);  /* dev = 20 kHz */

    /* RF power: 10 dBm at 868 MHz */
    cc1101_write_reg(cc, CC1101_FREND0, 0x10);
    cc1101_write_reg(cc, CC1101_PATABLE, CC1101_PATABLE_VAL);

    /* MCSM: auto-calibrate when going from IDLE to RX/TX */
    cc1101_write_reg(cc, CC1101_MCSM0, 0x18);
    cc1101_write_reg(cc, CC1101_MCSM1, 0x3F);  /* CCA, RX after TX */
    cc1101_write_reg(cc, CC1101_MCSM2, 0x07);

    /* AGC: optimized for low data rate */
    cc1101_write_reg(cc, CC1101_AGCCTRL2, 0x43);
    cc1101_write_reg(cc, CC1101_AGCCTRL1, 0x49);
    cc1101_write_reg(cc, CC1101_AGCCTRL0, 0x91);

    /* Frequency offset compensation */
    cc1101_write_reg(cc, CC1101_FOCCFG, 0x16);

    /* Calibration */
    cc1101_write_reg(cc, CC1101_FSCAL3, 0xEA);
    cc1101_write_reg(cc, CC1101_FSCAL2, 0x2A);
    cc1101_write_reg(cc, CC1101_FSCAL1, 0x00);
    cc1101_write_reg(cc, CC1101_FSCAL0, 0x11);

    /* FIFO threshold: 8 bytes RX, 8 bytes TX */
    cc1101_write_reg(cc, CC1101_FIFOTHR, 0x07);

    /* Calibrate and enter RX mode */
    cc1101_strobe(cc, CC1101_SCAL);
    vTaskDelay(pdMS_TO_TICKS(1));
    cc1101_rx_mode(cc);

    ESP_LOGI(TAG, "CC1101 initialized at 868 MHz, addr=0x%02X", addr);
    return 0;
}

/* ── TX / RX ────────────────────────────────────────────────── */

int cc1101_send(cc1101_t *cc, const uint8_t *data, uint8_t len)
{
    if (len > 61) {
        ESP_LOGE(TAG, "packet too long: %d", len);
        return -1;
    }

    /* Go to IDLE, flush TX FIFO */
    cc1101_strobe(cc, CC1101_SIDLE);
    cc1101_strobe(cc, CC1101_SFTX);

    /* Write packet to TX FIFO: length + address + payload + CRC(auto) */
    uint8_t fifo[64];
    fifo[0] = len + 1;       /* packet length (excludes length byte itself) */
    fifo[1] = cc->my_addr;  /* transmitter address (for receiver filter) */
    memcpy(&fifo[2], data, len);

    cc1101_write_burst(cc, CC1101_TXFIFO, fifo, len + 2);

    /* Enable TX */
    cc1101_strobe(cc, CC1101_STX);

    /* Wait for TX to complete (check TXBYTES or poll GD2) */
    int timeout = 0;
    while (timeout < 100) {
        uint8_t txbytes = cc1101_read_status(cc, CC1101_TXBYTES);
        if ((txbytes & 0x7F) == 0)
            break;
        vTaskDelay(pdMS_TO_TICKS(1));
        timeout++;
    }

    /* Return to RX mode */
    cc1101_strobe(cc, CC1101_SRX);

    return (timeout < 100) ? 0 : -2;
}

int cc1101_recv(cc1101_t *cc, uint8_t *data, uint8_t *len, int8_t *rssi)
{
    /* Check if packet in RX FIFO */
    uint8_t rxbytes = cc1101_read_status(cc, CC1101_RXBYTES) & 0x7F;
    if (rxbytes == 0)
        return -1;

    /* Read length byte */
    spi_transaction_t t = {0};
    uint8_t tx_fifo[66];
    uint8_t rx_fifo[66] = {0};
    tx_fifo[0] = CC1101_RXFIFO | CC1101_READ_BIT | CC1101_BURST_BIT;
    t.length = (rxbytes + 1) * 8;
    t.tx_buffer = tx_fifo;
    t.rx_buffer = rx_fifo;
    spi_device_polling_transmit(cc->spi, &t);

    /* First byte = packet length, second = dest addr (filtered by HW),
     * third onward = payload */
    uint8_t pkt_len = rx_fifo[1];
    if (pkt_len == 0 || pkt_len > 61) {
        cc1101_strobe(cc, CC1101_SFRX);
        return -2;
    }

    *len = pkt_len;
    memcpy(data, &rx_fifo[3], pkt_len);

    /* RSSI byte follows payload (append status enabled) */
    if (rssi && rxbytes >= pkt_len + 4) {
        uint8_t rssi_raw = rx_fifo[3 + pkt_len];
        /* Convert: RSSI = (rssi_raw - 128) / 2 - 74 if rssi_raw >= 128 */
        /*           RSSI = rssi_raw / 2 - 74 if rssi_raw < 128 */
        if (rssi_raw >= 128)
            *rssi = (int8_t)((rssi_raw - 256) / 2 - 74);
        else
            *rssi = (int8_t)(rssi_raw / 2 - 74);
    }

    /* Flush RX FIFO */
    cc1101_strobe(cc, CC1101_SFRX);
    cc1101_strobe(cc, CC1101_SRX);

    return 0;
}

int cc1101_rx_mode(cc1101_t *cc)
{
    cc1101_strobe(cc, CC1101_SIDLE);
    cc1101_strobe(cc, CC1101_SFRX);
    cc1101_strobe(cc, CC1101_SRX);
    return 0;
}

int cc1101_idle(cc1101_t *cc)
{
    cc1101_strobe(cc, CC1101_SIDLE);
    return 0;
}

int cc1101_sleep(cc1101_t *cc)
{
    cc1101_strobe(cc, CC1101_SIDLE);
    cc1101_strobe(cc, CC1101_SWOR);  /* Wake-on-Radio or power-down */
    cc1101_strobe(cc, CC1101_SPWD);
    return 0;
}

int8_t cc1101_get_rssi(cc1101_t *cc)
{
    uint8_t rssi_raw = cc1101_read_status(cc, CC1101_RSSI);
    if (rssi_raw >= 128)
        return (int8_t)((rssi_raw - 256) / 2 - 74);
    else
        return (int8_t)(rssi_raw / 2 - 74);
}

int cc1101_set_power(cc1101_t *cc, uint8_t dbm)
{
    uint8_t patable_val;
    /* Approximate PA table values for 868 MHz */
    switch (dbm) {
        case 0:  patable_val = 0x50; break;
        case 5:  patable_val = 0x84; break;
        case 7:  patable_val = 0x9A; break;
        case 10: patable_val = 0xC2; break;
        default: patable_val = 0xC2; break;
    }
    return cc1101_write_reg(cc, CC1101_PATABLE, patable_val);
}