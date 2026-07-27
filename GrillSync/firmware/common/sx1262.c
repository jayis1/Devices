/*
 * GrillSync — SX1262 Sub-GHz Radio Driver
 * Semtech SX1262 LoRa transceiver driver (portable C)
 */
#include "sx1262.h"

/* === Low-level SPI helpers === */

static void sx_write_reg(gs_radio_ctx_t *ctx, uint16_t addr, uint8_t val)
{
    ctx->spi->cs_select();
    ctx->spi->transfer(SX1262_CMD_WRITE_REGISTER);
    ctx->spi->transfer((uint8_t)(addr >> 8));
    ctx->spi->transfer((uint8_t)(addr & 0xFF));
    ctx->spi->transfer(val);
    ctx->spi->cs_release();
}

static uint8_t sx_read_reg(gs_radio_ctx_t *ctx, uint16_t addr)
{
    ctx->spi->cs_select();
    ctx->spi->transfer(SX1262_CMD_READ_REGISTER);
    ctx->spi->transfer((uint8_t)(addr >> 8));
    ctx->spi->transfer((uint8_t)(addr & 0xFF));
    uint8_t val = ctx->spi->transfer(0x00);
    ctx->spi->cs_release();
    return val;
}

static void sx_write_buffer(gs_radio_ctx_t *ctx, uint8_t offset,
                             const uint8_t *data, uint8_t len)
{
    ctx->spi->cs_select();
    ctx->spi->transfer(SX1262_CMD_WRITE_BUFFER);
    ctx->spi->transfer(offset);
    for (uint8_t i = 0; i < len; i++)
        ctx->spi->transfer(data[i]);
    ctx->spi->cs_release();
}

static void sx_read_buffer(gs_radio_ctx_t *ctx, uint8_t offset,
                            uint8_t *data, uint8_t len)
{
    ctx->spi->cs_select();
    ctx->spi->transfer(SX1262_CMD_READ_BUFFER);
    ctx->spi->transfer(offset);
    ctx->spi->transfer(0x00); /* dummy */
    for (uint8_t i = 0; i < len; i++)
        data[i] = ctx->spi->transfer(0x00);
    ctx->spi->cs_release();
}

static void sx_send_cmd(gs_radio_ctx_t *ctx, const uint8_t *cmd, uint8_t len)
{
    ctx->spi->cs_select();
    for (uint8_t i = 0; i < len; i++)
        ctx->spi->transfer(cmd[i]);
    ctx->spi->cs_release();
}

static void sx_wait_busy(gs_radio_ctx_t *ctx)
{
    /* In production: poll BUSY pin. Here, short delay fallback */
    ctx->spi->delay_ms(1);
}

static void sx_set_standby(gs_radio_ctx_t *ctx, uint8_t mode)
{
    uint8_t cmd[] = { SX1262_CMD_SET_STANDBY, mode };
    sx_send_cmd(ctx, cmd, sizeof(cmd));
    sx_wait_busy(ctx);
}

/* === Public API === */

int gs_radio_init(gs_radio_ctx_t *ctx, const gs_spi_interface_t *spi,
                  const gs_radio_config_t *config)
{
    ctx->spi = spi;
    ctx->config = *config;
    ctx->dio1_irq_status = 0;

    /* Hardware reset */
    spi->reset(1);
    spi->delay_ms(10);
    spi->reset(0);
    spi->delay_ms(50);

    /* Set standby */
    sx_set_standby(ctx, SX1262_STDBY_RC);

    /* Set regulator mode (DC-DC) */
    uint8_t reg_cmd[] = { SX1262_CMD_SET_REGULATOR_MODE, 0x01 };
    sx_send_cmd(ctx, reg_cmd, sizeof(reg_cmd));

    /* Set packet type to LoRa */
    uint8_t pkt_cmd[] = { SX1262_CMD_SET_PACKET_TYPE, SX1262_PKT_TYPE_LORA };
    sx_send_cmd(ctx, pkt_cmd, sizeof(pkt_cmd));

    /* Set RF frequency: freq = (freq_hz * 2^25) / 32MHz */
    uint32_t frf = ((uint64_t)config->frequency << 25) / 32000000ULL;
    uint8_t freq_cmd[] = {
        SX1262_CMD_SET_RF_FREQUENCY,
        (uint8_t)(frf >> 24),
        (uint8_t)(frf >> 16),
        (uint8_t)(frf >> 8),
        (uint8_t)(frf & 0xFF)
    };
    sx_send_cmd(ctx, freq_cmd, sizeof(freq_cmd));

    /* Set modulation params: SF, BW, CR */
    uint8_t bw_code;
    switch (config->bandwidth) {
        case 125000: bw_code = 0x04; break;  /* 125 kHz */
        case 250000: bw_code = 0x05; break;  /* 250 kHz */
        case 500000: bw_code = 0x06; break;  /* 500 kHz */
        default:    bw_code = 0x04; break;
    }
    uint8_t mod_cmd[] = {
        SX1262_CMD_SET_MODULATION_PARAMS,
        config->spreading_factor,
        bw_code,
        config->coding_rate
    };
    sx_send_cmd(ctx, mod_cmd, sizeof(mod_cmd));

    /* Set packet params: preamble, header type, payload length, CRC */
    uint8_t pkt_param_cmd[] = {
        SX1262_CMD_SET_PACKET_PARAMS,
        (uint8_t)(config->preamble_len >> 8),
        (uint8_t)(config->preamble_len & 0xFF),  /* Preamble length */
        0x00,  /* Explicit header */
        0x00,  /* Payload length (set per TX) */
        0x01,  /* CRC enable */
        0x00,  /* Invert IQ: standard */
        0x00, 0x00
    };
    sx_send_cmd(ctx, pkt_param_cmd, sizeof(pkt_param_cmd));

    /* Set TX power */
    gs_radio_set_tx_power(ctx, config->tx_power_dbm);

    /* Set DIO2 as RF switch */
    uint8_t dio2_cmd[] = { SX1262_CMD_SET_DIO2_AS_RF_SWITCH, 0x01 };
    sx_send_cmd(ctx, dio2_cmd, sizeof(dio2_cmd));

    /* Set IRQ params: enable TX_DONE and RX_DONE */
    uint8_t irq_cmd[] = {
        SX1262_CMD_SET_DIO_IRQ_PARAMS,
        0x03, 0x03,  /* IRQ mask (TX_DONE | RX_DONE) */
        0x03, 0x03,  /* DIO1 mask */
    };
    sx_send_cmd(ctx, irq_cmd, sizeof(irq_cmd));

    /* Clear any pending IRQs */
    uint8_t clr_cmd[] = { SX1262_CMD_CLEAR_IRQ_STATUS, 0xFF, 0xFF };
    sx_send_cmd(ctx, clr_cmd, sizeof(clr_cmd));

    return 0;
}

int gs_radio_tx(gs_radio_ctx_t *ctx, const uint8_t *data, uint8_t len)
{
    sx_set_standby(ctx, SX1262_STDBY_RC);

    /* Set buffer base address to 0 */
    uint8_t base_cmd[] = { SX1262_CMD_SET_BUFFER_BASE_ADDR, 0x00, 0x00 };
    sx_send_cmd(ctx, base_cmd, sizeof(base_cmd));

    /* Write data to buffer */
    sx_write_buffer(ctx, 0, data, len);

    /* Update packet params with actual payload length */
    uint8_t pkt_param_cmd[] = {
        SX1262_CMD_SET_PACKET_PARAMS,
        (uint8_t)(ctx->config.preamble_len >> 8),
        (uint8_t)(ctx->config.preamble_len & 0xFF),
        0x00,
        len,   /* Payload length */
        0x01,
        0x00, 0x00, 0x00
    };
    sx_send_cmd(ctx, pkt_param_cmd, sizeof(pkt_param_cmd));

    /* Clear IRQ status */
    uint8_t clr_cmd[] = { SX1262_CMD_CLEAR_IRQ_STATUS, 0xFF, 0xFF };
    sx_send_cmd(ctx, clr_cmd, sizeof(clr_cmd));

    /* Set TX with timeout (0 = no timeout) */
    uint8_t timeout = 0;
    uint8_t tx_cmd[] = {
        SX1262_CMD_SET_TX,
        timeout, timeout, timeout
    };
    sx_send_cmd(ctx, tx_cmd, sizeof(tx_cmd));
    sx_wait_busy(ctx);

    /* Wait for TX_DONE (poll DIO1 in production) */
    ctx->spi->delay_ms(100);

    return (int)len;
}

int gs_radio_rx(gs_radio_ctx_t *ctx, uint8_t *data, uint8_t max_len,
                uint32_t timeout_ms)
{
    /* Clear IRQ status */
    uint8_t clr_cmd[] = { SX1262_CMD_CLEAR_IRQ_STATUS, 0xFF, 0xFF };
    sx_send_cmd(ctx, clr_cmd, sizeof(clr_cmd));

    /* Set RX with timeout */
    uint32_t timeout = timeout_ms * 64; /* SX1262 timeout unit = 15.625µs */
    uint8_t rx_cmd[] = {
        SX1262_CMD_SET_RX,
        (uint8_t)(timeout >> 16),
        (uint8_t)(timeout >> 8),
        (uint8_t)(timeout & 0xFF)
    };
    sx_send_cmd(ctx, rx_cmd, sizeof(rx_cmd));
    sx_wait_busy(ctx);

    /* Wait for RX_DONE or timeout */
    ctx->spi->delay_ms(timeout_ms > 0 ? timeout_ms : 100);

    /* Check IRQ status */
    ctx->spi->cs_select();
    ctx->spi->transfer(SX1262_CMD_GET_IRQ_STATUS);
    uint8_t irq_lo = ctx->spi->transfer(0x00);
    uint8_t irq_hi = ctx->spi->transfer(0x00);
    ctx->spi->cs_release();

    if (!(irq_lo & 0x02))  /* RX_DONE not set */
        return -1;

    /* Get RX buffer status */
    uint8_t status[2];
    ctx->spi->cs_select();
    ctx->spi->transfer(SX1262_CMD_GET_RX_BUFFER_STATUS);
    status[0] = ctx->spi->transfer(0x00); /* Payload length */
    status[1] = ctx->spi->transfer(0x00); /* Buffer offset */
    ctx->spi->cs_release();

    uint8_t payload_len = status[0];
    uint8_t payload_off  = status[1];
    if (payload_len > max_len)
        payload_len = max_len;

    /* Read received data */
    sx_read_buffer(ctx, payload_off, data, payload_len);

    /* Clear IRQ status */
    uint8_t clr_cmd2[] = { SX1262_CMD_CLEAR_IRQ_STATUS, 0xFF, 0xFF };
    sx_send_cmd(ctx, clr_cmd2, sizeof(clr_cmd2));

    return (int)payload_len;
}

int gs_radio_set_tx_power(gs_radio_ctx_t *ctx, int8_t power_dbm)
{
    uint8_t cmd[] = {
        SX1262_CMD_SET_TX_PARAMS,
        (uint8_t)power_dbm,
        0x04   /* Ramp time 200µs */
    };
    sx_send_cmd(ctx, cmd, sizeof(cmd));
    ctx->config.tx_power_dbm = power_dbm;
    return 0;
}

int gs_radio_set_frequency(gs_radio_ctx_t *ctx, uint32_t freq_hz)
{
    uint32_t frf = ((uint64_t)freq_hz << 25) / 32000000ULL;
    uint8_t cmd[] = {
        SX1262_CMD_SET_RF_FREQUENCY,
        (uint8_t)(frf >> 24),
        (uint8_t)(frf >> 16),
        (uint8_t)(frf >> 8),
        (uint8_t)(frf & 0xFF)
    };
    sx_send_cmd(ctx, cmd, sizeof(cmd));
    ctx->config.frequency = freq_hz;
    return 0;
}

int8_t gs_radio_get_rssi(gs_radio_ctx_t *ctx)
{
    ctx->spi->cs_select();
    ctx->spi->transfer(SX1262_CMD_GET_RSSI_INST);
    uint8_t rssi = ctx->spi->transfer(0x00);
    ctx->spi->cs_release();
    return -(int8_t)(rssi / 2);
}

void gs_radio_reset(gs_radio_ctx_t *ctx)
{
    ctx->spi->reset(1);
    ctx->spi->delay_ms(10);
    ctx->spi->reset(0);
    ctx->spi->delay_ms(50);
}