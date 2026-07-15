/*
 * LawnSync — SX1262 Sub-GHz Radio Driver
 * LoRa transceiver driver for Semtech SX1262 (compatible with SX1276 API)
 *
 * This is a portable driver that uses a platform-specific SPI interface
 * provided by each node's board support package.
 */
#include "sx1262.h"

/* SX1262 register addresses and commands */
#define SX1262_REG_PKT_STATUS     0x14
#define SX1262_REG_RSSI_SYNC      0x15
#define SX1262_CMD_SET_STANDBY    0x80
#define SX1262_CMD_SET_FS        0xC0
#define SX1262_CMD_SET_TX        0x83
#define SX1262_CMD_SET_RX        0x82
#define SX1262_CMD_SET_SLEEP      0x84
#define SX1262_CMD_SET_CAD       0xC5
#define SX1262_CMD_SET_BW        0x96
#define SX1262_CMD_SET_RF_FREQ   0x86
#define SX1262_CMD_SET_TX_PARAMS 0x8E
#define SX1262_CMD_SET_MOD_PARAMS 0x8B
#define SX1262_CMD_SET_PACKET_TYPE 0x8A
#define SX1262_CMD_SET_PACKET_PARAMS 0x8C
#define SX1262_CMD_SET_CAD_PARAMS 0x88
#define SX1262_CMD_WRITE_BUFFER  0x0E
#define SX1262_CMD_READ_BUFFER   0x1E
#define SX1262_CMD_GET_STATUS    0xC0
#define SX1262_CMD_GET_RX_BUFFER_STATUS 0x13
#define SX1262_CMD_GET_PACKET_STATUS 0x14
#define SX1262_CMD_SET_DIO_IRQ   0x08
#define SX1262_CMD_CLEAR_IRQ     0x02
#define SX1262_CMD_GET_IRQ_STATUS 0x12

#define SX1262_PACKET_TYPE_LORA   0x01
#define SX1262_IRQ_TX_DONE       0x0001
#define SX1262_IRQ_RX_DONE       0x0002
#define SX1262_IRQ_CAD_DONE      0x0004
#define SX1262_IRQ_CAD_DETECTED  0x0008
#define SX1262_IRQ_TIMEOUT       0x0020
#define SX1262_IRQ_ALL           0x03FF

static const ls_spi_interface_t *g_spi = NULL;
static ls_radio_config_t g_config;
static ls_radio_state_t g_state = LS_RADIO_SLEEP;

/* Low-level SPI write */
static void sx_write(uint16_t addr, const uint8_t *data, uint8_t len)
{
    g_spi->cs_select();
    g_spi->transfer((uint8_t)(addr >> 8));
    g_spi->transfer((uint8_t)(addr & 0xFF));
    for (uint8_t i = 0; i < len; i++)
        g_spi->transfer(data[i]);
    g_spi->cs_release();
}

/* Low-level SPI read */
static void sx_read(uint16_t addr, uint8_t *data, uint8_t len)
{
    g_spi->cs_select();
    g_spi->transfer((uint8_t)(addr >> 8));
    g_spi->transfer((uint8_t)(addr & 0xFF));
    g_spi->transfer(0x00); /* dummy byte */
    for (uint8_t i = 0; i < len; i++)
        data[i] = g_spi->transfer(0x00);
    g_spi->cs_release();
}

/* Send a command */
static void sx_cmd(uint8_t cmd, const uint8_t *params, uint8_t param_len)
{
    g_spi->cs_select();
    g_spi->transfer(cmd);
    for (uint8_t i = 0; i < param_len; i++)
        g_spi->transfer(params[i]);
    g_spi->cs_release();
}

/* Wait for BUSY to go low */
static void sx_wait_busy(void)
{
    /* In a real implementation, read BUSY pin. For now, delay. */
    g_spi->delay_ms(1);
}

/* Set RF frequency */
static void set_rf_freq(uint32_t freq)
{
    uint32_t f = (uint32_t)(((uint64_t)freq << 25) / 32000000ULL);
    uint8_t params[4];
    params[0] = (uint8_t)(f >> 24);
    params[1] = (uint8_t)(f >> 16);
    params[2] = (uint8_t)(f >> 8);
    params[3] = (uint8_t)(f & 0xFF);
    sx_cmd(SX1262_CMD_SET_RF_FREQ, params, 4);
}

/* Set modulation parameters (LoRa: SF, BW, CR, LDRO) */
static void set_mod_params(void)
{
    uint8_t bw;
    switch (g_config.bandwidth) {
        case 62500:  bw = 0x20; break;
        case 125000: bw = 0x40; break;
        case 250000: bw = 0x60; break;
        case 500000: bw = 0x80; break;
        default:     bw = 0x40; break;
    }
    uint8_t ldro = (g_config.spreading_factor >= 11) ? 1 : 0;
    uint8_t params[4] = {
        g_config.spreading_factor,
        bw,
        g_config.coding_rate,
        ldro
    };
    sx_cmd(SX1262_CMD_SET_MOD_PARAMS, params, 4);
}

/* Set packet parameters (preamble, header type, payload length, CRC) */
static void set_packet_params_tx(uint8_t payload_len)
{
    uint8_t params[9] = {
        0x00, 0x00,                     /* preamble length MSB, LSB (8) */
        g_config.preamble_len,
        0x01,                           /* explicit header */
        payload_len,                    /* payload length */
        0x01,                           /* CRC on */
        0x00,                           /* standard preamble */
        0x00, 0x00
    };
    sx_cmd(SX1262_CMD_SET_PACKET_PARAMS, params, 9);
}

static void set_packet_params_rx(void)
{
    uint8_t params[9] = {
        0x00, 0x00,
        g_config.preamble_len,
        0x01,                           /* explicit header */
        0xFF,                           /* max payload length */
        0x01,                           /* CRC on */
        0x00,
        0x00, 0x00
    };
    sx_cmd(SX1262_CMD_SET_PACKET_PARAMS, params, 9);
}

/* Set TX power */
static void set_tx_power(int8_t power_dbm)
{
    uint8_t params[3] = {
        (uint8_t)(power_dbm + 2),      /* ramp time = 0x04 (200us) */
        0x04,                          /* ramp */
        0x00                           /* PA config */
    };
    sx_cmd(SX1262_CMD_SET_TX_PARAMS, params, 3);
}

int ls_radio_init(const ls_spi_interface_t *spi, const ls_radio_config_t *cfg)
{
    if (!spi || !cfg) return -1;
    g_spi = spi;
    g_config = *cfg;

    /* Initialize SPI interface */
    spi->init();

    /* Hardware reset */
    spi->reset(1);
    spi->delay_ms(10);
    spi->reset(0);
    spi->delay_ms(50);

    /* Set standby (RC oscillator) */
    uint8_t standby_rc[1] = {0x00};
    sx_cmd(SX1262_CMD_SET_STANDBY, standby_rc, 1);
    spi->delay_ms(10);

    /* Set packet type to LoRa */
    uint8_t pkt_type[1] = {SX1262_PACKET_TYPE_LORA};
    sx_cmd(SX1262_CMD_SET_PACKET_TYPE, pkt_type, 1);

    /* Set RF frequency */
    set_rf_freq(g_config.frequency);

    /* Set modulation parameters */
    set_mod_params();

    /* Set TX power */
    set_tx_power(g_config.tx_power_dbm);

    /* Clear all IRQ flags */
    uint8_t irq_clear[3] = {0xFF, 0xFF, 0x00};
    sx_cmd(SX1262_CMD_CLEAR_IRQ, irq_clear, 3);

    /* Enable TX_DONE and RX_DONE IRQs on DIO1 */
    uint8_t dio_irq[6] = {
        0x00, 0x03,   /* IRQ mask: TX_DONE | RX_DONE */
        0x00, 0x03,   /* DIO1 mask */
        0x00, 0x00    /* DIO2 mask (none) */
    };
    sx_cmd(SX1262_CMD_SET_DIO_IRQ, dio_irq, 6);

    g_state = LS_RADIO_IDLE;
    return 0;
}

int ls_radio_tx(const uint8_t *data, uint8_t len)
{
    if (!data || len == 0 || len > 255 || g_state == LS_RADIO_SLEEP) return -1;

    sx_wait_busy();

    /* Set to standby */
    uint8_t standby[1] = {0x00};
    sx_cmd(SX1262_CMD_SET_STANDBY, standby, 1);
    g_spi->delay_ms(1);

    /* Set packet params for TX */
    set_packet_params_tx(len);

    /* Write data to TX buffer */
    uint8_t offset_cmd[2] = {0x00, 0x00}; /* offset = 0 */
    sx_cmd(0x0E, offset_cmd, 2); /* WRITE_BUFFER command + offset */
    /* Actually need to write data after offset */
    g_spi->cs_select();
    g_spi->transfer(SX1262_CMD_WRITE_BUFFER);
    g_spi->transfer(0x00); /* offset */
    for (uint8_t i = 0; i < len; i++)
        g_spi->transfer(data[i]);
    g_spi->cs_release();

    /* Set TX with timeout (0 = no timeout, wait for TX done) */
    uint8_t tx_params[3] = {0x00, 0x00, 0x00}; /* timeout = 0 (no timeout) */
    sx_cmd(SX1262_CMD_SET_TX, tx_params, 3);
    g_state = LS_RADIO_TX;

    /* Wait for TX_DONE (poll DIO1) */
    uint32_t timeout = 5000; /* 5s max */
    while (timeout > 0) {
        if (g_spi->dio1_read()) break;
        g_spi->delay_ms(1);
        timeout--;
    }

    /* Clear IRQ */
    uint8_t irq_clear[3] = {0xFF, 0xFF, 0x00};
    sx_cmd(SX1262_CMD_CLEAR_IRQ, irq_clear, 3);

    /* Return to standby */
    sx_cmd(SX1262_CMD_SET_STANDBY, standby, 1);
    g_state = LS_RADIO_IDLE;

    return (timeout > 0) ? (int)len : -1;
}

int ls_radio_rx(ls_radio_packet_t *pkt, uint32_t timeout_ms)
{
    if (!pkt || g_state == LS_RADIO_SLEEP) return -1;

    sx_wait_busy();

    uint8_t standby[1] = {0x00};
    sx_cmd(SX1262_CMD_SET_STANDBY, standby, 1);
    g_spi->delay_ms(1);

    /* Set packet params for RX */
    set_packet_params_rx();

    /* Clear IRQ flags */
    uint8_t irq_clear[3] = {0xFF, 0xFF, 0x00};
    sx_cmd(SX1262_CMD_CLEAR_IRQ, irq_clear, 3);

    /* Set RX with timeout */
    uint32_t timeout = (timeout_ms * 64); /* SX1262 timeout in 15.625us units */
    uint8_t rx_params[3] = {
        (uint8_t)(timeout >> 16),
        (uint8_t)(timeout >> 8),
        (uint8_t)(timeout & 0xFF)
    };
    sx_cmd(SX1262_CMD_SET_RX, rx_params, 3);
    g_state = LS_RADIO_RX;

    /* Wait for RX_DONE or TIMEOUT */
    uint32_t waited = 0;
    while (waited < timeout_ms + 100) {
        if (g_spi->dio1_read()) break;
        g_spi->delay_ms(1);
        waited++;
    }

    if (waited >= timeout_ms + 100) {
        sx_cmd(SX1262_CMD_SET_STANDBY, standby, 1);
        g_state = LS_RADIO_IDLE;
        return -1; /* timeout */
    }

    /* Clear IRQ */
    sx_cmd(SX1262_CMD_CLEAR_IRQ, irq_clear, 3);

    /* Get RX buffer status */
    uint8_t rx_status[2] = {0, 0};
    sx_read(SX1262_CMD_GET_RX_BUFFER_STATUS, rx_status, 2);
    uint8_t payload_len = rx_status[0];
    uint8_t rx_offset = rx_status[1];

    /* Read payload from buffer */
    g_spi->cs_select();
    g_spi->transfer(SX1262_CMD_READ_BUFFER);
    g_spi->transfer(rx_offset);
    g_spi->transfer(0x00); /* dummy */
    for (uint8_t i = 0; i < payload_len && i < 255; i++)
        pkt->data[i] = g_spi->transfer(0x00);
    g_spi->cs_release();
    pkt->data_len = payload_len;

    /* Get packet status (RSSI, SNR) */
    uint8_t pkt_status[3] = {0, 0, 0};
    sx_read(SX1262_CMD_GET_PACKET_STATUS, pkt_status, 3);
    pkt->rssi = pkt_status[0]; /* RSSI in dBm (two's complement) */
    pkt->snr  = pkt_status[1]; /* SNR in dB */

    /* Return to standby */
    sx_cmd(SX1262_CMD_SET_STANDBY, standby, 1);
    g_state = LS_RADIO_IDLE;

    return (int)payload_len;
}

int ls_radio_sleep(void)
{
    uint8_t sleep_params[1] = {0x04}; /* cold start, RTC disabled */
    sx_cmd(SX1262_CMD_SET_SLEEP, sleep_params, 1);
    g_state = LS_RADIO_SLEEP;
    g_spi->delay_ms(1);
    return 0;
}

int ls_radio_wakeup(void)
{
    /* Toggle CS to wake */
    g_spi->cs_select();
    g_spi->delay_ms(1);
    g_spi->cs_release();

    uint8_t standby[1] = {0x00};
    sx_cmd(SX1262_CMD_SET_STANDBY, standby, 1);
    g_state = LS_RADIO_IDLE;
    return 0;
}

int ls_radio_cad(uint32_t timeout_ms)
{
    /* CAD params: symbols, peak, min, exit mode, timeout */
    uint8_t cad_params[7] = {
        g_config.spreading_factor, /* CAD symbols = SF */
        0x0A,  /* CAD detect peak: 10 */
        0x0A,  /* CAD detect min: 10 */
        0x01,  /* exit mode: CAD only */
        0x00, 0x00, 0x00 /* timeout */
    };
    sx_cmd(SX1262_CMD_SET_CAD_PARAMS, cad_params, 7);

    /* Clear IRQ */
    uint8_t irq_clear[3] = {0xFF, 0xFF, 0x00};
    sx_cmd(SX1262_CMD_CLEAR_IRQ, irq_clear, 3);

    /* Start CAD */
    sx_cmd(SX1262_CMD_SET_CAD, NULL, 0);
    g_state = LS_RADIO_CAD;

    /* Wait for CAD done */
    uint32_t waited = 0;
    while (waited < timeout_ms + 100) {
        if (g_spi->dio1_read()) break;
        g_spi->delay_ms(1);
        waited++;
    }

    /* Read IRQ status to check if CAD detected */
    uint8_t irq_status[2] = {0, 0};
    sx_read(SX1262_CMD_GET_IRQ_STATUS, irq_status, 2);
    uint16_t irq = (uint16_t)irq_status[0] | ((uint16_t)irq_status[1] << 8);

    sx_cmd(SX1262_CMD_CLEAR_IRQ, irq_clear, 3);

    uint8_t standby[1] = {0x00};
    sx_cmd(SX1262_CMD_SET_STANDBY, standby, 1);
    g_state = LS_RADIO_IDLE;

    return (irq & SX1262_IRQ_CAD_DETECTED) ? 1 : 0;
}

int8_t ls_radio_get_rssi(void)
{
    uint8_t rssi = 0;
    sx_read(SX1262_REG_RSSI_SYNC, &rssi, 1);
    return (int8_t)rssi;
}

int ls_radio_set_config(const ls_radio_config_t *cfg)
{
    if (!cfg) return -1;
    g_config = *cfg;

    uint8_t standby[1] = {0x00};
    sx_cmd(SX1262_CMD_SET_STANDBY, standby, 1);

    set_rf_freq(g_config.frequency);
    set_mod_params();
    set_tx_power(g_config.tx_power_dbm);

    return 0;
}