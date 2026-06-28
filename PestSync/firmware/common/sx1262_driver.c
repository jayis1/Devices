/*
 * SX1262 Sub-GHz LoRa Driver — Implementation
 * sx1262_driver.c
 * Simplified reference driver for SX1262 (Semtech) over SPI.
 * In production, use the official SX126x library.
 */
#include "sx1262_driver.h"
#include "psp_protocol.h"
#include <string.h>

/* SX1262 SPI commands */
#define SX1262_CMD_SET_SLEEP        0x84
#define SX1262_CMD_SET_STANDBY      0x80
#define SX1262_CMD_SET_TX           0x83
#define SX1262_CMD_SET_RX           0x82
#define SX1262_CMD_WRITE_BUFFER     0x0E
#define SX1262_CMD_READ_BUFFER      0x1E
#define SX1262_CMD_SET_RF_FREQ      0x86
#define SX1262_CMD_SET_TX_PARAMS    0x8E
#define SX1262_CMD_SET_MOD_PARAMS   0x8B
#define SX1262_CMD_SET_PACKET_PARAMS 0x8C
#define SX1262_CMD_SET_SYNC_WORD    0x07
#define SX1262_CMD_GET_RX_BUFFER    0x13
#define SX1262_CMD_CLEAR_IRQ        0x02
#define SX1262_CMD_GET_IRQ_STATUS   0x12
#define SX1262_CMD_SET_DIO2_AS_RF_SW 0x9D

/* Standby config */
#define SX1262_STDBY_RC   0x00
#define SX1262_STDBY_XOSC 0x01

static void sx1262_write_reg(sx1262_t *r, uint8_t cmd, const uint8_t *data, size_t len)
{
    r->hal.cs_select();
    uint8_t tx[1 + 16];
    uint8_t rx[1 + 16];
    tx[0] = cmd;
    if (data && len <= 16) memcpy(tx + 1, data, len);
    r->hal.spi_transfer(tx, rx, 1 + len);
    r->hal.cs_deselect();
}

static void sx1262_read_reg(sx1262_t *r, uint8_t cmd, uint8_t *data, size_t len)
{
    r->hal.cs_select();
    uint8_t tx[1 + 16 + 4] = {0};
    uint8_t rx[1 + 16 + 4] = {0};
    tx[0] = cmd;
    /* SX1262 read has 4 dummy bytes before data for some commands */
    r->hal.spi_transfer(tx, rx, 1 + 4 + len);
    if (data && len <= 16) memcpy(data, rx + 1 + 4, len);
    r->hal.cs_deselect();
}

static void sx1262_write_buffer(sx1262_t *r, uint8_t offset, const uint8_t *data, size_t len)
{
    r->hal.cs_select();
    uint8_t tx[2 + 256];
    uint8_t rx[2 + 256];
    tx[0] = SX1262_CMD_WRITE_BUFFER;
    tx[1] = offset;
    if (data && len <= 256) memcpy(tx + 2, data, len);
    r->hal.spi_transfer(tx, rx, 2 + len);
    r->hal.cs_deselect();
}

static void sx1262_read_buffer(sx1262_t *r, uint8_t offset, uint8_t *data, size_t len)
{
    r->hal.cs_select();
    uint8_t tx[3 + 256] = {0};
    uint8_t rx[3 + 256] = {0};
    tx[0] = SX1262_CMD_READ_BUFFER;
    tx[1] = offset;
    /* 2 dummy bytes before data */
    r->hal.spi_transfer(tx, rx, 3 + len);
    if (data && len <= 256) memcpy(data, rx + 3, len);
    r->hal.cs_deselect();
}

int sx1262_init(sx1262_t *radio)
{
    /* Reset */
    radio->hal.reset(true);
    radio->hal.millis(); /* brief delay */
    radio->hal.reset(false);
    radio->hal.wait_busy();

    /* Set standby */
    uint8_t standby = SX1262_STDBY_RC;
    sx1262_write_reg(radio, SX1262_CMD_SET_STANDBY, &standby, 1);
    radio->hal.wait_busy();

    /* Set RF frequency: 868 MHz
     * freq = (freq_hz * 2^25) / 32 MHz
     * 868000000 * 33554432 / 32000000 = 0x0A4EC4C4 (approx)
     */
    uint32_t freq_word = (uint32_t)((uint64_t)radio->freq_hz * (1ULL << 25) / 32000000ULL);
    uint8_t freq_cmd[4] = {
        (uint8_t)(freq_word >> 24),
        (uint8_t)(freq_word >> 16),
        (uint8_t)(freq_word >> 8),
        (uint8_t)(freq_word)
    };
    sx1262_write_reg(radio, SX1262_CMD_SET_RF_FREQ, freq_cmd, 4);
    radio->hal.wait_busy();

    /* Set TX power: 17 dBm, ramp 200 us */
    uint8_t tx_params[2] = { (uint8_t)radio->tx_power_dbm, 0x04 };
    sx1262_write_reg(radio, SX1262_CMD_SET_TX_PARAMS, tx_params, 2);

    /* Set DIO2 as RF switch control */
    uint8_t dio2 = 0x01;
    sx1262_write_reg(radio, SX1262_CMD_SET_DIO2_AS_RF_SW, &dio2, 1);

    /* Set sync word */
    uint8_t sync[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    sync[0] = radio->sync_word;
    sx1262_write_reg(radio, SX1262_CMD_SET_SYNC_WORD, sync, 6);

    /* Clear IRQ */
    uint8_t clear[2] = { 0xFF, 0xFF };
    sx1262_write_reg(radio, SX1262_CMD_CLEAR_IRQ, clear, 2);

    return 0;
}

void sx1262_set_key(sx1262_t *radio, const uint8_t *key)
{
    if (key) {
        memcpy(radio->aes_key, key, 16);
        radio->aes_set = true;
    }
}

int sx1262_tx(sx1262_t *radio, const uint8_t *data, size_t len)
{
    if (len > 255) return -1;

    /* Write data to buffer at offset 0 */
    sx1262_write_buffer(radio, 0, data, len);

    /* Set packet params: preamble 8, header explicit, payload length, CRC on */
    uint8_t pkt_params[9] = {
        0x00, 0x08,             /* preamble length 8 */
        0x00,                   /* explicit header */
        (uint8_t)len,           /* payload length */
        0x01,                   /* CRC on */
        0x00, 0x00, 0x00, 0x00  /* reserved */
    };
    sx1262_write_reg(radio, SX1262_CMD_SET_PACKET_PARAMS, pkt_params, 9);

    /* Set TX with timeout (0 = no timeout, 3 bytes */
    uint8_t tx_cmd[3] = { 0x00, 0x00, 0x00 };
    sx1262_write_reg(radio, SX1262_CMD_SET_TX, tx_cmd, 3);

    /* Wait for DIO1 (TX done) */
    uint32_t start = radio->hal.millis();
    while (!radio->hal.get_dio1()) {
        if (radio->hal.millis() - start > 3000) return -2;
    }

    /* Clear IRQ */
    uint8_t clear[2] = { 0xFF, 0xFF };
    sx1262_write_reg(radio, SX1262_CMD_CLEAR_IRQ, clear, 2);

    return 0;
}

int sx1262_rx(sx1262_t *radio, uint32_t timeout_ms, uint8_t *data, size_t max_len, int *rssi)
{
    /* Set RX with timeout */
    uint32_t timeout_ticks = timeout_ms * 64; /* SX1262 timeout in 15.625 us units */
    uint8_t rx_cmd[3] = {
        (uint8_t)(timeout_ticks >> 16),
        (uint8_t)(timeout_ticks >> 8),
        (uint8_t)(timeout_ticks)
    };
    sx1262_write_reg(radio, SX1262_CMD_SET_RX, rx_cmd, 3);

    /* Wait for DIO1 (RX done) */
    uint32_t start = radio->hal.millis();
    while (!radio->hal.get_dio1()) {
        if (radio->hal.millis() - start > timeout_ms + 100) return -1;
    }

    /* Get RX buffer status */
    uint8_t status[2] = {0};
    sx1262_read_reg(radio, SX1262_CMD_GET_RX_BUFFER, status, 2);

    uint8_t payload_len = status[0];
    uint8_t offset = status[1];

    if (payload_len > max_len) payload_len = (uint8_t)max_len;
    if (payload_len == 0) return -2;

    /* Read payload */
    sx1262_read_buffer(radio, offset, data, payload_len);

    /* Clear IRQ */
    uint8_t clear[2] = { 0xFF, 0xFF };
    sx1262_write_reg(radio, SX1262_CMD_CLEAR_IRQ, clear, 2);

    if (rssi) {
        /* Approximate RSSI: read from IRQ status or packet status */
        *rssi = -80; /* placeholder — real impl reads PacketStatus */
    }

    return payload_len;
}

void sx1262_sleep(sx1262_t *radio)
{
    uint8_t sleep_cfg = 0x04; /* warm start, RTC disabled */
    sx1262_write_reg(radio, SX1262_CMD_SET_SLEEP, &sleep_cfg, 1);
}

void sx1262_set_tx_power(sx1262_t *radio, int8_t dbm)
{
    radio->tx_power_dbm = dbm;
    uint8_t tx_params[2] = { (uint8_t)dbm, 0x04 };
    sx1262_write_reg(radio, SX1262_CMD_SET_TX_PARAMS, tx_params, 2);
}