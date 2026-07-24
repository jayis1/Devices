/*
 * EchoSync — SX1262 Sub-GHz Radio Driver (Implementation)
 * Semtech SX1262 LoRa transceiver driver
 *
 * This is a platform-abstracted driver. Platform-specific SPI glue
 * is provided by each node's main.c via the es_spi_interface_t struct.
 */
#include "sx1262.h"
#include "config.h"

/* SX1262 register addresses */
#define SX1262_REG_PKT_SNR        0x0267
#define SX1262_REG_PKT_RSSI       0x0268
#define SX1262_REG_SYNC_WORD      0x0740

/* SX1262 commands */
#define SX1262_CMD_SET_STANDBY    0x80
#define SX1262_CMD_SET_PACKET_TYPE 0x8A
#define SX1262_CMD_SET_RF_FREQ    0x86
#define SX1262_CMD_SET_TX_PARAMS  0x8E
#define SX1262_CMD_SET_MODULATION 0x8B
#define SX1262_CMD_SET_PACKET_PARAMS 0x8C
#define SX1262_CMD_SET_TX         0x83
#define SX1262_CMD_SET_RX         0x82
#define SX1262_CMD_WRITE_BUFFER   0x0D
#define SX1262_CMD_READ_BUFFER    0x1D
#define SX1262_CMD_GET_RX_BUFFER  0x13
#define SX1262_CMD_CLEAR_IRQ       0x02
#define SX1262_CMD_SET_DIO_IRQ    0x08
#define SX1262_CMD_SET_DIO2_RF_SW  0x9D

static es_radio_ctx_t g_radio;

/* Low-level SPI write */
static void sx_write(uint8_t addr, const uint8_t *data, uint8_t len)
{
    g_radio.spi->cs_select();
    g_radio.spi->transfer(addr);
    for (uint8_t i = 0; i < len; i++)
        g_radio.spi->transfer(data[i]);
    g_radio.spi->cs_release();
}

/* Low-level SPI read */
static void sx_read(uint8_t addr, uint8_t *data, uint8_t len)
{
    g_radio.spi->cs_select();
    g_radio.spi->transfer(addr);
    g_radio.spi->transfer(0x00); /* dummy */
    for (uint8_t i = 0; i < len; i++)
        data[i] = g_radio.spi->transfer(0x00);
    g_radio.spi->cs_release();
}

/* Write command */
static void sx_command(uint8_t cmd, const uint8_t *params, uint8_t len)
{
    g_radio.spi->cs_select();
    g_radio.spi->transfer(cmd);
    for (uint8_t i = 0; i < len; i++)
        g_radio.spi->transfer(params[i]);
    g_radio.spi->cs_release();
}

/* Wait for BUSY to deassert */
static void sx_wait_busy(void)
{
    /* In production: poll BUSY pin; simplified here */
}

/* Initialize radio */
int es_radio_init(es_radio_ctx_t *ctx, const es_spi_interface_t *spi,
                  const es_radio_config_t *cfg)
{
    if (!ctx || !spi || !cfg) return -1;
    ctx->spi = spi;
    ctx->config = *cfg;
    g_radio = *ctx;

    spi->init();

    /* Reset radio */
    spi->reset(1);
    spi->delay_ms(10);
    spi->reset(0);
    spi->delay_ms(10);

    /* Set standby */
    uint8_t standby = 0x00; /* STDBY_RC */
    sx_command(SX1262_CMD_SET_STANDBY, &standby, 1);
    spi->delay_ms(1);

    /* Set packet type to LoRa */
    uint8_t pkt_type = 0x01;
    sx_command(SX1262_CMD_SET_PACKET_TYPE, &pkt_type, 1);

    /* Set RF frequency (Hz → 32-bit register, freq = Hz * 2^25 / 32MHz) */
    uint32_t rf_freq = (uint32_t)((double)cfg->frequency * 33554432.0 / 32000000.0);
    uint8_t freq_buf[4];
    freq_buf[0] = (uint8_t)(rf_freq >> 24);
    freq_buf[1] = (uint8_t)(rf_freq >> 16);
    freq_buf[2] = (uint8_t)(rf_freq >> 8);
    freq_buf[3] = (uint8_t)(rf_freq);
    sx_command(SX1262_CMD_SET_RF_FREQ, freq_buf, 4);

    /* Set TX power */
    uint8_t tx_params[2] = { (uint8_t)cfg->tx_power_dbm, 0x04 /* ramp 200us */ };
    sx_command(SX1262_CMD_SET_TX_PARAMS, tx_params, 2);

    /* Set modulation params: SF, BW, CR, LDRO */
    uint8_t mod_params[4];
    mod_params[0] = cfg->spreading_factor;
    uint32_t bw = cfg->bandwidth;
    if (bw == 125000) mod_params[1] = 0x04;
    else if (bw == 250000) mod_params[1] = 0x05;
    else if (bw == 500000) mod_params[1] = 0x06;
    else mod_params[1] = 0x04;
    mod_params[2] = cfg->coding_rate;
    mod_params[3] = (cfg->spreading_factor >= 11) ? 0x01 : 0x00; /* LDRO */
    sx_command(SX1262_CMD_SET_MODULATION, mod_params, 4);

    /* Set packet params: preamble, header type, payload length, CRC */
    uint8_t pkt_params[9];
    pkt_params[0] = 0x00; /* preamble MSB */
    pkt_params[1] = cfg->preamble_len;
    pkt_params[2] = 0x00; /* explicit header */
    pkt_params[3] = 64;  /* max payload */
    pkt_params[4] = 0x01; /* CRC on */
    pkt_params[5] = 0x00; /* invertIQ off */
    pkt_params[6] = 0x00;
    pkt_params[7] = 0x00;
    pkt_params[8] = 0x00;
    sx_command(SX1262_CMD_SET_PACKET_PARAMS, pkt_params, 9);

    /* Clear IRQ status */
    uint8_t clear[2] = { 0xFF, 0xFF };
    sx_command(SX1262_CMD_CLEAR_IRQ, clear, 2);

    ctx->initialized = 1;
    return 0;
}

/* Transmit data */
int es_radio_tx(const uint8_t *data, uint8_t len)
{
    if (!data || len == 0 || !g_radio.initialized) return -1;

    /* Write buffer at offset 0 */
    uint8_t write_cmd[1 + 64];
    write_cmd[0] = 0x00; /* offset */
    memcpy(&write_cmd[1], data, len);
    sx_write(SX1262_CMD_WRITE_BUFFER, write_cmd, 1 + len);

    /* Set TX with timeout */
    uint32_t timeout = 0x000100; /* ~1 second */
    uint8_t tx_cmd[3] = {
        (uint8_t)(timeout >> 16),
        (uint8_t)(timeout >> 8),
        (uint8_t)(timeout)
    };
    sx_command(SX1262_CMD_SET_TX, tx_cmd, 3);

    /* Wait for TX done (DIO1 or poll) */
    g_radio.spi->delay_ms(100);

    /* Clear IRQ */
    uint8_t clear[2] = { 0xFF, 0xFF };
    sx_command(SX1262_CMD_CLEAR_IRQ, clear, 2);

    return len;
}

/* Receive data */
int es_radio_rx(uint8_t *buf, uint8_t max_len, uint32_t timeout_ms)
{
    if (!buf || !g_radio.initialized) return -1;

    /* Set RX with timeout */
    uint32_t timeout = timeout_ms * 64; /* ms → SX1262 ticks (~15.625us) */
    uint8_t rx_cmd[3] = {
        (uint8_t)(timeout >> 16),
        (uint8_t)(timeout >> 8),
        (uint8_t)(timeout)
    };
    sx_command(SX1262_CMD_SET_RX, rx_cmd, 3);

    /* Wait for DIO1 (RX done) or timeout */
    uint32_t elapsed = 0;
    while (elapsed < timeout_ms) {
        if (g_radio.spi->dio1_read()) {
            /* Read received buffer */
            uint8_t offset_status[2] = { 0x00, 0x00 };
            sx_read(SX1262_CMD_GET_RX_BUFFER, offset_status, 2);
            uint8_t offset = offset_status[0];
            uint8_t rx_len = offset_status[1];
            if (rx_len > max_len) rx_len = max_len;

            /* Read buffer at offset */
            uint8_t read_cmd[1] = { offset };
            sx_write(SX1262_CMD_READ_BUFFER, read_cmd, 1);
            /* Actually need to read: use read command */
            g_radio.spi->cs_select();
            g_radio.spi->transfer(SX1262_CMD_READ_BUFFER);
            g_radio.spi->transfer(offset);
            g_radio.spi->transfer(0x00); /* dummy */
            for (uint8_t i = 0; i < rx_len; i++)
                buf[i] = g_radio.spi->transfer(0x00);
            g_radio.spi->cs_release();

            /* Clear IRQ */
            uint8_t clear[2] = { 0xFF, 0xFF };
            sx_command(SX1262_CMD_CLEAR_IRQ, clear, 2);

            return rx_len;
        }
        g_radio.spi->delay_ms(10);
        elapsed += 10;
    }

    /* Timeout */
    uint8_t clear[2] = { 0xFF, 0xFF };
    sx_command(SX1262_CMD_CLEAR_IRQ, clear, 2);
    return 0;
}

/* Get RSSI */
int8_t es_radio_get_rssi(void)
{
    uint8_t rssi;
    sx_read(SX1262_REG_PKT_RSSI, &rssi, 1);
    return (int8_t)(-(int8_t)(rssi / 2));
}

/* Set TX power */
void es_radio_set_tx_power(int8_t dbm)
{
    uint8_t tx_params[2] = { (uint8_t)dbm, 0x04 };
    sx_command(SX1262_CMD_SET_TX_PARAMS, tx_params, 2);
}

/* Sleep */
void es_radio_sleep(void)
{
    uint8_t sleep_cfg = 0x04; /* cold start, RTC disabled */
    sx_command(0x84, &sleep_cfg, 1);
}

/* Wakeup */
void es_radio_wakeup(void)
{
    /* Toggle CS to wake */
    g_radio.spi->cs_select();
    g_radio.spi->cs_release();
    g_radio.spi->delay_ms(1);
}