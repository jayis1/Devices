/*
 * VoiceSync — SX1262 Radio Driver (Implementation)
 * Semtech SX1262 Sub-GHz LoRa transceiver
 *
 * This is a simplified driver showing the register-level interface.
 * In production, use the official Semtech SX126x driver adapted to
 * the platform's SPI/GPIO abstraction.
 */
#include "sx1262.h"
#include <string.h>

/* SX1262 SPI commands */
#define SX1262_CMD_SET_SLEEP      0x84
#define SX1262_CMD_SET_STANDBY    0x80
#define SX1262_CMD_SET_RX         0x82
#define SX1262_CMD_SET_TX         0x83
#define SX1262_CMD_WRITE_REGISTER 0x0D
#define SX1262_CMD_READ_REGISTER  0x1D
#define SX1262_CMD_WRITE_BUFFER   0x0E
#define SX1262_CMD_READ_BUFFER    0x1E
#define SX1262_CMD_SET_MOD_PARAMS 0x8B
#define SX1262_CMD_SET_RF_FREQ    0x86
#define SX1262_CMD_SET_TX_PARAMS  0x8E
#define SX1262_CMD_SET_PACKET_TYPE 0x8A
#define SX1262_CMD_SET_CAD        0x85
#define SX1262_CMD_GET_RX_BUFFER  0x13
#define SX1262_CMD_GET_STATUS     0xC0
#define SX1262_CMD_CLEAR_IRQ      0x02
#define SX1262_CMD_SET_DIO_IRQ    0x08

/* SX1262 register addresses */
#define SX1262_REG_RX_TX_BUFFER  0x00
#define SX1262_REG_LORA_SYNC     0x0740
#define SX1262_REG_LORA_PREAMBLE 0x087F

static const vs_spi_interface_t *g_spi = NULL;
static vs_radio_state_t g_state = VS_RADIO_SLEEP;
static int8_t g_last_rssi = -100;

/* Internal: write register */
static void write_reg(uint16_t addr, uint8_t val)
{
    g_spi->cs_select();
    g_spi->transfer(SX1262_CMD_WRITE_REGISTER);
    g_spi->transfer((uint8_t)(addr >> 8));
    g_spi->transfer((uint8_t)(addr & 0xFF));
    g_spi->transfer(val);
    g_spi->cs_release();
}

/* Internal: read register */
static uint8_t read_reg(uint16_t addr)
{
    g_spi->cs_select();
    g_spi->transfer(SX1262_CMD_READ_REGISTER);
    g_spi->transfer((uint8_t)(addr >> 8));
    g_spi->transfer((uint8_t)(addr & 0xFF));
    uint8_t val = g_spi->transfer(0x00);
    g_spi->cs_release();
    return val;
}

/* Internal: write buffer */
static void write_buffer(uint8_t offset, const uint8_t *data, uint8_t len)
{
    g_spi->cs_select();
    g_spi->transfer(SX1262_CMD_WRITE_BUFFER);
    g_spi->transfer(offset);
    for (uint8_t i = 0; i < len; i++)
        g_spi->transfer(data[i]);
    g_spi->cs_release();
}

/* Internal: read buffer */
static void read_buffer(uint8_t offset, uint8_t *data, uint8_t len)
{
    g_spi->cs_select();
    g_spi->transfer(SX1262_CMD_READ_BUFFER);
    g_spi->transfer(offset);
    for (uint8_t i = 0; i < len; i++)
        data[i] = g_spi->transfer(0x00);
    g_spi->cs_release();
}

/* Internal: wait while busy */
static void wait_busy(void)
{
    /* In production: poll BUSY pin with timeout */
    g_spi->delay_ms(1);
}

/* Internal: set standby mode */
static void set_standby(void)
{
    g_spi->cs_select();
    g_spi->transfer(SX1262_CMD_SET_STANDBY);
    g_spi->transfer(0x01); /* STDBY_RC */
    g_spi->cs_release();
    wait_busy();
    g_state = VS_RADIO_IDLE;
}

int vs_radio_init(const vs_spi_interface_t *spi, const vs_radio_config_t *cfg)
{
    if (!spi || !cfg) return -1;
    g_spi = spi;

    /* Reset radio */
    spi->reset(1);
    spi->delay_ms(10);
    spi->reset(0);
    spi->delay_ms(10);

    /* Set standby */
    set_standby();

    /* Set packet type to LoRa */
    g_spi->cs_select();
    g_spi->transfer(SX1262_CMD_SET_PACKET_TYPE);
    g_spi->transfer(0x01); /* LoRa */
    g_spi->cs_release();
    wait_busy();

    /* Set RF frequency */
    uint32_t freq_reg = (uint32_t)((double)cfg->frequency / 32.0);
    g_spi->cs_select();
    g_spi->transfer(SX1262_CMD_SET_RF_FREQ);
    g_spi->transfer((uint8_t)(freq_reg >> 24));
    g_spi->transfer((uint8_t)(freq_reg >> 16));
    g_spi->transfer((uint8_t)(freq_reg >> 8));
    g_spi->transfer((uint8_t)(freq_reg & 0xFF));
    g_spi->cs_release();
    wait_busy();

    /* Set modulation parameters */
    uint8_t sf = cfg->spreading_factor;
    uint8_t bw;
    switch (cfg->bandwidth) {
        case 125000: bw = 0x04; break;  /* 125 kHz */
        case 250000: bw = 0x05; break;  /* 250 kHz */
        case 500000: bw = 0x06; break;  /* 500 kHz */
        default:     bw = 0x04; break;
    }
    g_spi->cs_select();
    g_spi->transfer(SX1262_CMD_SET_MOD_PARAMS);
    g_spi->transfer(sf);
    g_spi->transfer(bw);
    g_spi->transfer(cfg->coding_rate);
    g_spi->transfer(0x00); /* Low data rate optimize off */
    g_spi->cs_release();
    wait_busy();

    /* Set TX power */
    g_spi->cs_select();
    g_spi->transfer(SX1262_CMD_SET_TX_PARAMS);
    g_spi->transfer((uint8_t)cfg->tx_power_dbm);
    g_spi->transfer(0x04); /* Ramp 200 us */
    g_spi->cs_release();
    wait_busy();

    /* Set preamble length */
    write_reg(SX1262_REG_LORA_PREAMBLE, cfg->preamble_len);

    /* Clear IRQ */
    g_spi->cs_select();
    g_spi->transfer(SX1262_CMD_CLEAR_IRQ);
    g_spi->transfer(0xFF);
    g_spi->transfer(0xFF);
    g_spi->cs_release();

    g_state = VS_RADIO_IDLE;
    return 0;
}

int vs_radio_tx(const uint8_t *data, uint8_t len)
{
    if (!g_spi || !data || len == 0) return -1;

    set_standby();

    /* Write data to buffer at offset 0 */
    write_buffer(0, data, len);

    /* Set IRQ for TX done */
    g_spi->cs_select();
    g_spi->transfer(SX1262_CMD_SET_DIO_IRQ);
    g_spi->transfer(0x01); g_spi->transfer(0x00); /* IRQ mask: TxDone */
    g_spi->transfer(0x01); g_spi->transfer(0x00); /* DIO1 mask */
    g_spi->cs_release();
    wait_busy();

    /* Set TX with timeout (30 seconds) */
    g_spi->cs_select();
    g_spi->transfer(SX1262_CMD_SET_TX);
    g_spi->transfer(0x00); g_spi->transfer(0x07);
    g_spi->transfer(0xD0); g_spi->transfer(0x00); /* 30s timeout */
    g_spi->cs_release();
    wait_busy();

    g_state = VS_RADIO_TX;

    /* Wait for TX complete (poll DIO1 in production) */
    uint32_t timeout = 0;
    while (g_spi->dio1_read() == 0 && timeout < 30000) {
        g_spi->delay_ms(1);
        timeout++;
    }

    /* Clear IRQ */
    g_spi->cs_select();
    g_spi->transfer(SX1262_CMD_CLEAR_IRQ);
    g_spi->transfer(0xFF);
    g_spi->transfer(0xFF);
    g_spi->cs_release();

    set_standby();
    return (int)len;
}

int vs_radio_rx(uint8_t *buf, uint8_t max_len, uint32_t timeout_ms)
{
    if (!g_spi || !buf) return -1;

    /* Set IRQ for RX done */
    g_spi->cs_select();
    g_spi->transfer(SX1262_CMD_SET_DIO_IRQ);
    g_spi->transfer(0x02); g_spi->transfer(0x00); /* IRQ mask: RxDone */
    g_spi->transfer(0x02); g_spi->transfer(0x00); /* DIO1 mask */
    g_spi->cs_release();
    wait_busy();

    /* Set RX with timeout */
    uint32_t ticks = timeout_ms * 64; /* 15.625 us per tick */
    g_spi->cs_select();
    g_spi->transfer(SX1262_CMD_SET_RX);
    g_spi->transfer((uint8_t)(ticks >> 24));
    g_spi->transfer((uint8_t)(ticks >> 16));
    g_spi->transfer((uint8_t)(ticks >> 8));
    g_spi->transfer((uint8_t)(ticks & 0xFF));
    g_spi->cs_release();
    wait_busy();

    g_state = VS_RADIO_RX;

    /* Wait for RX complete or timeout */
    uint32_t elapsed = 0;
    while (g_spi->dio1_read() == 0 && elapsed < timeout_ms) {
        g_spi->delay_ms(1);
        elapsed++;
    }

    if (elapsed >= timeout_ms) {
        set_standby();
        return 0; /* Timeout */
    }

    /* Read packet status: start addr + length */
    uint8_t status[3];
    g_spi->cs_select();
    g_spi->transfer(SX1262_CMD_GET_RX_BUFFER);
    g_spi->transfer(0x00); g_spi->transfer(0x00);
    status[0] = g_spi->transfer(0x00); /* Start offset */
    status[1] = g_spi->transfer(0x00); /* Length */
    status[2] = g_spi->transfer(0x00); /* RSSI */
    g_spi->cs_release();

    uint8_t pkt_len = status[1];
    if (pkt_len > max_len) pkt_len = max_len;

    /* Read received data */
    read_buffer(status[0], buf, pkt_len);

    /* Read RSSI from packet status */
    g_last_rssi = (int8_t)status[2] - 157; /* SX1262 RSSI offset */

    /* Clear IRQ */
    g_spi->cs_select();
    g_spi->transfer(SX1262_CMD_CLEAR_IRQ);
    g_spi->transfer(0xFF);
    g_spi->transfer(0xFF);
    g_spi->cs_release();

    set_standby();
    return (int)pkt_len;
}

int8_t vs_radio_get_rssi(void)
{
    return g_last_rssi;
}

void vs_radio_sleep(void)
{
    g_spi->cs_select();
    g_spi->transfer(SX1262_CMD_SET_SLEEP);
    g_spi->transfer(0x04); /* Warm start */
    g_spi->cs_release();
    g_state = VS_RADIO_SLEEP;
}

int vs_radio_wakeup(void)
{
    /* Toggle reset to wake */
    g_spi->reset(1);
    g_spi->delay_ms(10);
    g_spi->reset(0);
    g_spi->delay_ms(10);
    set_standby();
    return 0;
}

vs_radio_state_t vs_radio_get_state(void)
{
    return g_state;
}