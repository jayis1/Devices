/*
 * SX1262 LoRa Radio Driver implementation
 * sx1262_driver.c
 */
#include "sx1262_driver.h"
#include <string.h>

static void sx1262_write_reg(sx1262_t *r, uint16_t addr, uint8_t val)
{
    uint8_t tx[4] = { SX1262_CMD_WRITE_BUF >> 8, addr >> 8, addr & 0xFF, val };
    /* Simplified — real impl uses WRITE_BUF command with offset */
    r->hal.cs_select();
    r->hal.wait_busy();
    r->hal.spi_transfer(tx, NULL, 4);
    r->hal.cs_deselect();
}

static void sx1262_write_cmd(sx1262_t *r, uint8_t cmd, const uint8_t *buf, size_t len)
{
    uint8_t tx[64];
    tx[0] = cmd;
    if (buf && len > 0)
        memcpy(tx + 1, buf, len);
    r->hal.cs_select();
    r->hal.wait_busy();
    r->hal.spi_transfer(tx, NULL, len + 1);
    r->hal.cs_deselect();
}

static void sx1262_read_cmd(sx1262_t *r, uint8_t cmd, uint8_t *buf, size_t len)
{
    uint8_t tx[64] = { cmd, 0x00 }; /* command + dummy byte */
    r->hal.cs_select();
    r->hal.wait_busy();
    r->hal.spi_transfer(tx, NULL, 1);
    r->hal.spi_transfer(NULL, buf, len);
    r->hal.cs_deselect();
}

static void sx1262_write_buffer(sx1262_t *r, uint8_t offset,
                                 const uint8_t *data, size_t len)
{
    uint8_t tx[256];
    tx[0] = SX1262_CMD_WRITE_BUF;
    tx[1] = offset;
    memcpy(tx + 2, data, len);
    r->hal.cs_select();
    r->hal.wait_busy();
    r->hal.spi_transfer(tx, NULL, len + 2);
    r->hal.cs_deselect();
}

static void sx1262_read_buffer(sx1262_t *r, uint8_t offset,
                                uint8_t *data, size_t len)
{
    uint8_t tx[3] = { SX1262_CMD_READ_BUF, offset, 0x00 };
    r->hal.cs_select();
    r->hal.wait_busy();
    r->hal.spi_transfer(tx, NULL, 3);
    r->hal.spi_transfer(NULL, data, len);
    r->hal.cs_deselect();
}

static void sx1262_set_standby(sx1262_t *r, uint8_t mode)
{
    uint8_t buf[1] = { mode };
    sx1262_write_cmd(r, SX1262_CMD_SET_STANDBY, buf, 1);
}

int sx1262_init(sx1262_t *radio)
{
    /* Hardware reset */
    radio->hal.reset(true);
    radio->hal.millis(); /* delay */
    radio->hal.reset(false);
    radio->hal.wait_busy();

    /* Set standby RC */
    sx1262_set_standby(radio, 0x00);
    radio->hal.wait_busy();

    /* Set packet type: LoRa */
    uint8_t pkt_type[1] = { SX1262_PKT_TYPE_LORA };
    sx1262_write_cmd(r, SX1262_CMD_SET_PKT_TYPE, pkt_type, 1);

    /* Set RF frequency: 868 MHz
     * freq = (uint32_t)(868000000 / (32e6 / 2^25)) = (uint32_t)(868e6 * 2^25 / 32e6)
     * = (uint32_t)(868e6 * 33554432 / 32000000) = (uint32_t)(868 * 33554432 / 32000)
     * = (uint32_t)(291,252,556) approximately
     */
    sx1262_set_frequency(radio, LORA_FREQ);

    /* Set modulation config: SF=11, BW=125kHz, CR=4/5, LDRO=on */
    uint8_t mod_cfg[3] = {
        0x00,   /* LoRa BW 125 = 0x00 in SX1262 mapping (see datasheet table) */
        0x0B,   /* SF 11 */
        0x04    /* CR 4/5 + LDRO enabled */
    };
    sx1262_write_cmd(r, SX1262_CMD_SET_MOD_CFG, mod_cfg, 3);

    /* Set packet params: preamble=8, header=explicit, length=255, CRC=on, IQ=standard */
    uint8_t pkt_params[9] = {
        0x00, 0x08,  /* preamble length 8 */
        0x00,        /* explicit header */
        0xFF,        /* payload length (max) */
        0x01,        /* CRC enabled */
        0x00,        /* invert IQ: standard */
        0x00, 0x00, 0x00  /* reserved */
    };
    sx1262_write_cmd(r, SX1262_CMD_SET_PKT_PARAM, pkt_params, 9);

    /* Set TX power: 17 dBm with PA config for SX1262 */
    uint8_t pa_cfg[4] = { 0x04, 0x07, 0x00, 0x01 }; /* paDutyCycle, hpMax, deviceSel, paLut */
    sx1262_write_cmd(r, SX1262_CMD_SET_PA_CFG, pa_cfg, 4);

    uint8_t tx_params[3] = { 17, 0x04, 0x07 }; /* power 22 dBm, ramp 200us */
    sx1262_write_cmd(r, SX1262_CMD_SET_TX_PARAMS, tx_params, 3);

    radio->seq_num = 0;

    return 0;
}

int sx1262_set_frequency(sx1262_t *radio, uint32_t freq_hz)
{
    /* rf_freq = freq_hz * 2^25 / 32MHz */
    uint32_t rf_freq = (uint32_t)(((uint64_t)freq_hz << 25) / 32000000ULL);
    uint8_t buf[4];
    buf[0] = (rf_freq >> 24) & 0xFF;
    buf[1] = (rf_freq >> 16) & 0xFF;
    buf[2] = (rf_freq >>  8) & 0xFF;
    buf[3] = rf_freq & 0xFF;
    sx1262_write_cmd(radio, SX1262_CMD_SET_RF_FREQ, buf, 4);
    return 0;
}

void sx1262_set_key(sx1262_t *radio, const uint8_t *key)
{
    memcpy(radio->aes_key, key, AES_KEY_SIZE);
}

int sx1262_tx(sx1262_t *radio, uint16_t src, uint16_t dst,
              uint8_t msg_type, const uint8_t *payload, uint8_t len)
{
    uint8_t packet[CSP_MAX_PACKET];
    int pkt_len = csp_build_packet(packet, src, dst, msg_type, payload, len,
                                    radio->seq_num, radio->aes_key);
    if (pkt_len < 0)
        return -1;

    radio->seq_num++;

    /* Write packet to SX1262 buffer */
    sx1262_write_buffer(radio, 0, packet, (size_t)pkt_len);

    /* Set TX timeout: 0 = no timeout (TX until done) */
    uint8_t tx_cmd[3] = { 0x00, 0x00, 0x00 };
    sx1262_write_cmd(radio, SX1262_CMD_SET_TX, tx_cmd, 3);

    /* Wait for TX done (poll DIO1) */
    uint32_t start = radio->hal.millis();
    while (!radio->hal.get_dio1()) {
        if (radio->hal.millis() - start > 5000)
            return -2; /* timeout */
    }

    /* Clear IRQ */
    uint8_t irq_clear[3] = { 0x02, 0xFF, 0xFF };
    sx1262_write_cmd(radio, SX1262_CMD_CLEAR_IRQ, irq_clear, 3);

    return pkt_len;
}

int sx1262_rx(sx1262_t *radio, uint32_t timeout_ms, csp_header_t *hdr,
              uint8_t *payload, uint8_t *payload_len)
{
    /* Set RX timeout: convert ms to SX1262 units (15.625 µs per unit) */
    uint32_t timeout = timeout_ms * 64; /* approx */
    uint8_t rx_cmd[3] = {
        (uint8_t)(timeout >> 16),
        (uint8_t)(timeout >> 8),
        (uint8_t)(timeout & 0xFF)
    };
    sx1262_write_cmd(radio, SX1262_CMD_SET_RX, rx_cmd, 3);

    /* Wait for RX done or timeout */
    uint32_t start = radio->hal.millis();
    while (!radio->hal.get_dio1()) {
        if (radio->hal.millis() - start > timeout_ms + 1000)
            return -1;
    }

    /* Clear IRQ */
    uint8_t irq_clear[3] = { 0x02, 0xFF, 0xFF };
    sx1262_write_cmd(radio, SX1262_CMD_CLEAR_IRQ, irq_clear, 3);

    /* Read packet from buffer */
    uint8_t packet[CSP_MAX_PACKET];
    sx1262_read_buffer(radio, 0, packet, CSP_MAX_PACKET);

    /* Parse */
    return csp_parse_packet(packet, CSP_MAX_PACKET, hdr, payload, payload_len,
                            radio->aes_key);
}