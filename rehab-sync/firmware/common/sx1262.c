/*
 * RehabSync — SX1262 Sub-GHz Radio Driver
 * LoRa modulation, 868 MHz, TDMA mesh support.
 * Uses Semtech SX1262 via SPI.
 */
#include "sx1262.h"
#include "config.h"
#include <string.h>

/* SX1262 commands */
#define SX1262_CMD_SET_SLEEP       0x84
#define SX1262_CMD_SET_STANDBY     0x80
#define SX1262_CMD_SET_FS          0xC1
#define SX1262_CMD_SET_TX          0x83
#define SX1262_CMD_SET_RX          0x82
#define SX1262_CMD_STOP_TIMER      0x9F
#define SX1262_CMD_SET_RX_DUTY     0x94
#define SX1262_CMD_SET_CAD         0xC5
#define SX1262_CMD_SET_TX_CONT     0xD1
#define SX1262_CMD_SET_RX_CONT     0xD3
#define SX1262_CMD_SET_STBY_RC     0x80
#define SX1262_CMD_SET_STBY_XOSC   0x81

#define SX1262_CMD_SET_PACKET_TYPE 0x8A
#define SX1262_CMD_GET_PACKET_TYPE 0x03
#define SX1262_CMD_SET_RF_FREQ     0x86
#define SX1262_CMD_SET_TX_PARAMS   0x8E
#define SX1262_CMD_SET_MOD_PARAMS  0x8B
#define SX1262_CMD_SET_CAD_PARAMS  0x88
#define SX1262_CMD_SET_BUFFER_BASE 0x8F
#define SX1262_CMD_SET_PACKET_PARAM 0x8C

#define SX1262_CMD_WRITE_BUFFER    0x0E
#define SX1262_CMD_READ_BUFFER     0x1D
#define SX1262_CMD_GET_RX_BUFFER   0x13
#define SX1262_CMD_GET_PACKET_STATUS 0x1D

#define SX1262_CMD_SET_DIO_IRQ     0x08
#define SX1262_CMD_GET_IRQ_STATUS  0x12
#define SX1262_CMD_CLEAR_IRQ       0x02

#define SX1262_CMD_RESET           0x80
#define SX1262_CMD_SET_REGULATOR   0x89

/* Packet types */
#define SX1262_PKT_TYPE_LORA       0x01
#define SX1262_PKT_TYPE_FSK        0x02

/* IRQ flags */
#define SX1262_IRQ_TX_DONE         0x0001
#define SX1262_IRQ_RX_DONE         0x0002
#define SX1262_IRQ_PREAMBLE_DET    0x0004
#define SX1262_IRQ_SYNCWORD_DET    0x0008
#define SX1262_IRQ_HEADER_DET      0x0010
#define SX1262_IRQ_CRC_ERR         0x0020
#define SX1262_IRQ_CAD_DONE        0x0040
#define SX1262_IRQ_CAD_DET         0x0080
#define SX1262_IRQ_TIMEOUT         0x0100
#define SX1262_IRQ_ALL             0x03FF

static sx1262_t *g_active_radio = NULL;

int sx1262_init(sx1262_t *radio, const sx1262_config_t *cfg)
{
    memset(radio, 0, sizeof(*radio));
    radio->cfg = *cfg;
    g_active_radio = radio;

    /* Hardware reset */
    sx1262_reset(radio);
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Set standby mode (RC oscillator) */
    uint8_t standby = SX1262_CMD_SET_STBY_RC;
    sx1262_spi_write(radio, SX1262_CMD_SET_STANDBY, &standby, 1);
    radio->current_mode = SX1262_MODE_STBY_RC;
    vTaskDelay(pdMS_TO_TICKS(1));

    /* Set packet type: LoRa */
    uint8_t pkt_type = SX1262_PKT_TYPE_LORA;
    sx1262_spi_write(radio, SX1262_CMD_SET_PACKET_TYPE, &pkt_type, 1);

    /* Set RF frequency (868 MHz → 0x0E8D2E80 in 32-bit register, 32 MHz crystal) */
    /* freq_reg = freq_Hz * 2^25 / 32MHz */
    uint64_t freq_reg = ((uint64_t)cfg->frequency << 25) / 32000000ULL;
    uint8_t freq_cmd[4] = {
        (uint8_t)(freq_reg >> 24),
        (uint8_t)(freq_reg >> 16),
        (uint8_t)(freq_reg >> 8),
        (uint8_t)(freq_reg & 0xFF)
    };
    sx1262_spi_write(radio, SX1262_CMD_SET_RF_FREQ, freq_cmd, 4);

    /* Set TX power */
    uint8_t tx_params[2] = { (uint8_t)cfg->tx_power_dbm, 0x04 /* ramp 200us */ };
    sx1262_spi_write(radio, SX1262_CMD_SET_TX_PARAMS, tx_params, 2);

    /* Set LoRa modulation parameters: SF, BW, CR, LDRO */
    uint8_t bw_val = 0x0A; /* 250 kHz → 0x0A */
    uint8_t mod_params[4] = {
        cfg->spreading_factor,
        bw_val,
        cfg->coding_rate - 1, /* 4/5 → 0, etc. */
        0x01 /* LDRO enabled */
    };
    sx1262_spi_write(radio, SX1262_CMD_SET_MOD_PARAMS, mod_params, 4);

    /* Set LoRa packet params: preamble, header type, payload length, CRC */
    uint8_t pkt_params[9] = {
        (uint8_t)(cfg->preamble_len >> 8),
        (uint8_t)(cfg->preamble_len & 0xFF),
        0x01, /* explicit header */
        0x00, /* payload length (set per TX) */
        0x01, /* CRC on */
        0x00, /* standard IQ */
        0x00, 0x00, 0x00
    };
    sx1262_spi_write(radio, SX1262_CMD_SET_PACKET_PARAM, pkt_params, 9);

    /* Enable TX_DONE and RX_DONE + CRC_ERR + TIMEOUT IRQs */
    uint8_t irq_cfg[4] = {
        (uint8_t)((SX1262_IRQ_TX_DONE | SX1262_IRQ_RX_DONE | SX1262_IRQ_CRC_ERR | SX1262_IRQ_TIMEOUT) >> 8),
        (uint8_t)((SX1262_IRQ_TX_DONE | SX1262_IRQ_RX_DONE | SX1262_IRQ_CRC_ERR | SX1262_IRQ_TIMEOUT) & 0xFF),
        0x00, 0x00
    };
    sx1262_spi_write(radio, SX1262_CMD_SET_DIO_IRQ, irq_cfg, 4);

    /* Clear any pending IRQs */
    uint8_t clear[2] = { 0x03, 0xFF };
    sx1262_spi_write(radio, SX1262_CMD_CLEAR_IRQ, clear, 2);

    return 0;
}

int sx1262_set_mode(sx1262_t *radio, enum sx1262_mode mode)
{
    uint8_t cmd;
    switch (mode) {
        case SX1262_MODE_SLEEP:    cmd = SX1262_CMD_SET_SLEEP; break;
        case SX1262_MODE_STBY_RC:  cmd = SX1262_CMD_SET_STBY_RC; break;
        case SX1262_MODE_STBY_XOSC: cmd = SX1262_CMD_SET_STBY_XOSC; break;
        case SX1262_MODE_FS:       cmd = SX1262_CMD_SET_FS; break;
        case SX1262_MODE_TX:       cmd = SX1262_CMD_SET_TX; break;
        case SX1262_MODE_RX:       cmd = SX1262_CMD_SET_RX; break;
        default: return -1;
    }
    sx1262_spi_write(radio, cmd, NULL, 0);
    radio->current_mode = mode;
    return 0;
}

int sx1262_tx(sx1262_t *radio, const uint8_t *data, size_t len, uint32_t timeout_ms)
{
    if (len > 255) return -1;

    /* Set standby before writing buffer */
    sx1262_set_mode(radio, SX1262_MODE_STBY_RC);

    /* Write data to TX buffer at offset 0 */
    uint8_t offset_cmd[2] = { 0x00, 0x00 }; /* base addr = 0, ptr = 0 */
    sx1262_spi_write(radio, SX1262_CMD_SET_BUFFER_BASE, offset_cmd, 2);
    sx1262_spi_write(radio, SX1262_CMD_WRITE_BUFFER, data, len);

    /* Set payload length */
    /* (Set via packet params — simplified: update payload length field) */
    uint8_t pkt_params[9] = {
        (uint8_t)(radio->cfg.preamble_len >> 8),
        (uint8_t)(radio->cfg.preamble_len & 0xFF),
        0x01, /* explicit header */
        (uint8_t)len,
        0x01, /* CRC on */
        0x00, 0x00, 0x00, 0x00
    };
    sx1262_spi_write(radio, SX1262_CMD_SET_PACKET_PARAM, pkt_params, 9);

    /* Clear IRQs */
    uint8_t clear[2] = { 0x03, 0xFF };
    sx1262_spi_write(radio, SX1262_CMD_CLEAR_IRQ, clear, 2);

    /* Set TX with timeout */
    uint32_t timeout = timeout_ms * 64; /* 15.625 μs units */
    uint8_t tx_cmd[3] = {
        (uint8_t)(timeout >> 16),
        (uint8_t)(timeout >> 8),
        (uint8_t)(timeout & 0xFF)
    };
    sx1262_spi_write(radio, SX1262_CMD_SET_TX, tx_cmd, 3);
    radio->current_mode = SX1262_MODE_TX;

    /* Wait for TX_DONE (poll IRQ status — production uses DIO1 interrupt) */
    uint8_t irq_status[2] = {0, 0};
    for (uint32_t i = 0; i < timeout_ms * 10; i++) {
        sx1262_spi_read(radio, SX1262_CMD_GET_IRQ_STATUS, irq_status, 2);
        uint16_t irq = ((uint16_t)irq_status[0] << 8) | irq_status[1];
        if (irq & SX1262_IRQ_TX_DONE) {
            sx1262_spi_write(radio, SX1262_CMD_CLEAR_IRQ, clear, 2);
            radio->tx_count++;
            sx1262_set_mode(radio, SX1262_MODE_STBY_RC);
            return (int)len;
        }
        if (irq & SX1262_IRQ_TIMEOUT) {
            sx1262_spi_write(radio, SX1262_CMD_CLEAR_IRQ, clear, 2);
            sx1262_set_mode(radio, SX1262_MODE_STBY_RC);
            return -2; /* timeout */
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    sx1262_set_mode(radio, SX1262_MODE_STBY_RC);
    return -3;
}

int sx1262_rx(sx1262_t *radio, uint8_t *data, size_t cap, uint32_t timeout_ms)
{
    /* Clear IRQs */
    uint8_t clear[2] = { 0x03, 0xFF };
    sx1262_spi_write(radio, SX1262_CMD_CLEAR_IRQ, clear, 2);

    /* Set RX with timeout */
    uint32_t timeout = timeout_ms * 64;
    uint8_t rx_cmd[3] = {
        (uint8_t)(timeout >> 16),
        (uint8_t)(timeout >> 8),
        (uint8_t)(timeout & 0xFF)
    };
    sx1262_spi_write(radio, SX1262_CMD_SET_RX, rx_cmd, 3);
    radio->current_mode = SX1262_MODE_RX;

    /* Wait for RX_DONE */
    uint8_t irq_status[2] = {0, 0};
    for (uint32_t i = 0; i < timeout_ms * 10; i++) {
        sx1262_spi_read(radio, SX1262_CMD_GET_IRQ_STATUS, irq_status, 2);
        uint16_t irq = ((uint16_t)irq_status[0] << 8) | irq_status[1];
        if (irq & SX1262_IRQ_RX_DONE) {
            sx1262_spi_write(radio, SX1262_CMD_CLEAR_IRQ, clear, 2);

            /* Get RX buffer status */
            uint8_t rx_buf_status[2] = {0, 0};
            sx1262_spi_read(radio, SX1262_CMD_GET_RX_BUFFER, rx_buf_status, 2);
            uint8_t payload_len = rx_buf_status[0];
            uint8_t payload_off = rx_buf_status[1];

            if (payload_len > cap) payload_len = (uint8_t)cap;

            /* Read payload from buffer */
            uint8_t read_cmd = payload_off;
            sx1262_spi_write(radio, SX1262_CMD_SET_BUFFER_BASE, &read_cmd, 1);
            sx1262_spi_read(radio, SX1262_CMD_READ_BUFFER, data, payload_len);

            /* Get packet status (RSSI, SNR) */
            uint8_t pkt_status[3] = {0, 0, 0};
            sx1262_spi_read(radio, SX1262_CMD_GET_PACKET_STATUS, pkt_status, 3);
            radio->rssi = -((int8_t)pkt_status[0]) / 2; /* dBm */
            radio->snr  = ((int8_t)pkt_status[1]) / 4;  /* dB */

            radio->rx_count++;
            sx1262_set_mode(radio, SX1262_MODE_STBY_RC);
            return payload_len;
        }
        if (irq & SX1262_IRQ_CRC_ERR) {
            sx1262_spi_write(radio, SX1262_CMD_CLEAR_IRQ, clear, 2);
            radio->crc_errors++;
            sx1262_set_mode(radio, SX1262_MODE_STBY_RC);
            return -4; /* CRC error */
        }
        if (irq & SX1262_IRQ_TIMEOUT) {
            sx1262_spi_write(radio, SX1262_CMD_CLEAR_IRQ, clear, 2);
            sx1262_set_mode(radio, SX1262_MODE_STBY_RC);
            return -2; /* timeout */
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    sx1262_set_mode(radio, SX1262_MODE_STBY_RC);
    return -3;
}

int sx1262_cad(sx1262_t *radio)
{
    uint8_t clear[2] = { 0x03, 0xFF };
    sx1262_spi_write(radio, SX1262_CMD_CLEAR_IRQ, clear, 2);

    /* Set CAD params: symbol timeout = 1, exit mode = standby, timeout = 0 */
    uint8_t cad_params[7] = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    sx1262_spi_write(radio, SX1262_CMD_SET_CAD_PARAMS, cad_params, 7);

    sx1262_spi_write(radio, SX1262_CMD_SET_CAD, NULL, 0);

    uint8_t irq_status[2] = {0, 0};
    for (uint32_t i = 0; i < 100; i++) {
        sx1262_spi_read(radio, SX1262_CMD_GET_IRQ_STATUS, irq_status, 2);
        uint16_t irq = ((uint16_t)irq_status[0] << 8) | irq_status[1];
        if (irq & SX1262_IRQ_CAD_DONE) {
            sx1262_spi_write(radio, SX1262_CMD_CLEAR_IRQ, clear, 2);
            sx1262_set_mode(radio, SX1262_MODE_STBY_RC);
            return (irq & SX1262_IRQ_CAD_DET) ? 1 : 0;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    sx1262_set_mode(radio, SX1262_MODE_STBY_RC);
    return -1;
}

void sx1262_sleep(sx1262_t *radio)
{
    /* Warm start (preserve config) */
    uint8_t sleep_cfg = 0x04; /* enable warm start */
    sx1262_spi_write(radio, SX1262_CMD_SET_SLEEP, &sleep_cfg, 1);
    radio->current_mode = SX1262_MODE_SLEEP;
}