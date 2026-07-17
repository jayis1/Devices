/*
 * StormSync — SX1262 Radio Driver (Implementation)
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
#define SX1262_CMD_SET_LORA_MOD   0x01
#define SX1262_CMD_SET_CAD        0x85
#define SX1262_CMD_GET_RX_BUFFER  0x13
#define SX1262_CMD_GET_STATUS     0xC0
#define SX1262_CMD_CLEAR_IRQ      0x02
#define SX1262_CMD_SET_DIO_IRQ    0x08

/* SX1262 register addresses */
#define SX1262_REG_RX_TX_BUFFER  0x00
#define SX1262_REG_LORA_SYNC     0x0740
#define SX1262_REG_LORA_PREAMBLE 0x087F

static const ss_spi_interface_t *g_spi = NULL;
static ss_radio_state_t g_state = SS_RADIO_SLEEP;
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
    g_spi->transfer(0x00); /* status byte */
    for (uint8_t i = 0; i < len; i++)
        data[i] = g_spi->transfer(0x00);
    g_spi->cs_release();
}

/* Internal: set standby mode */
static void set_standby(void)
{
    g_spi->cs_select();
    g_spi->transfer(SX1262_CMD_SET_STANDBY);
    g_spi->transfer(0x01); /* STDBY_RC */
    g_spi->cs_release();
    g_state = SS_RADIO_IDLE;
    g_spi->delay_ms(1);
}

int ss_radio_init(const ss_spi_interface_t *spi, const ss_radio_config_t *cfg)
{
    if (!spi || !cfg) return -1;
    g_spi = spi;

    /* Reset radio */
    g_spi->reset(1);
    g_spi->delay_ms(10);
    g_spi->reset(0);
    g_spi->delay_ms(10);

    /* Set standby */
    set_standby();

    /* Set packet type: LoRa */
    g_spi->cs_select();
    g_spi->transfer(SX1262_CMD_SET_PACKET_TYPE);
    g_spi->transfer(SX1262_CMD_SET_LORA_MOD);
    g_spi->cs_release();

    /* Set RF frequency (4-byte register) */
    uint32_t rf_freq = (uint32_t)((uint64_t)cfg->frequency * (1ULL << 25) / 32000000ULL);
    g_spi->cs_select();
    g_spi->transfer(SX1262_CMD_SET_RF_FREQ);
    g_spi->transfer((uint8_t)(rf_freq >> 24));
    g_spi->transfer((uint8_t)(rf_freq >> 16));
    g_spi->transfer((uint8_t)(rf_freq >> 8));
    g_spi->transfer((uint8_t)(rf_freq & 0xFF));
    g_spi->cs_release();

    /* Set modulation parameters: SF, BW, CR, LDRO */
    g_spi->cs_select();
    g_spi->transfer(SX1262_CMD_SET_MOD_PARAMS);
    g_spi->transfer(cfg->spreading_factor);
    g_spi->transfer(0x04); /* BW 125 kHz = 0x04 in SX1262 encoding */
    g_spi->transfer(cfg->coding_rate);
    g_spi->transfer(0x00); /* LDRO disabled (auto) */
    g_spi->cs_release();

    /* Set TX power */
    g_spi->cs_select();
    g_spi->transfer(SX1262_CMD_SET_TX_PARAMS);
    g_spi->transfer((uint8_t)cfg->tx_power_dbm);
    g_spi->transfer(0x02); /* Ramp time 200 µs */
    g_spi->cs_release();

    /* Clear IRQ flags */
    g_spi->cs_select();
    g_spi->transfer(SX1262_CMD_CLEAR_IRQ);
    g_spi->transfer(0xFF);
    g_spi->transfer(0xFF);
    g_spi->cs_release();

    g_state = SS_RADIO_IDLE;
    return 0;
}

int ss_radio_tx(const uint8_t *data, uint8_t len)
{
    if (!g_spi || !data || len == 0) return -1;

    set_standby();

    /* Write data to buffer */
    write_buffer(0, data, len);

    /* Set TX: write buffer pointer + timeout */
    g_spi->cs_select();
    g_spi->transfer(SX1262_CMD_SET_TX);
    g_spi->transfer(0x00); /* timeout MSB (0 = no timeout) */
    g_spi->transfer(0x00);
    g_spi->transfer(0x00);
    g_spi->cs_release();

    g_state = SS_RADIO_TX;

    /* Wait for TX done (poll DIO1) — in production use ISR */
    uint32_t timeout = 5000;
    while (g_spi->dio1_read() == 0 && timeout > 0) {
        g_spi->delay_ms(1);
        timeout--;
    }

    /* Clear IRQ */
    g_spi->cs_select();
    g_spi->transfer(SX1262_CMD_CLEAR_IRQ);
    g_spi->transfer(0xFF);
    g_spi->transfer(0xFF);
    g_spi->cs_release();

    g_state = SS_RADIO_IDLE;
    return (timeout > 0) ? len : -1;
}

int ss_radio_rx(uint8_t *buf, uint8_t max_len, uint32_t timeout_ms)
{
    if (!g_spi || !buf) return -1;

    /* Set RX with timeout */
    uint32_t timeout = timeout_ms * 64; /* SX1262 timeout unit = 15.625µs */
    g_spi->cs_select();
    g_spi->transfer(SX1262_CMD_SET_RX);
    g_spi->transfer((uint8_t)(timeout >> 16));
    g_spi->transfer((uint8_t)(timeout >> 8));
    g_spi->transfer((uint8_t)(timeout & 0xFF));
    g_spi->cs_release();

    g_state = SS_RADIO_RX;

    /* Wait for RX done (poll DIO1) */
    uint32_t wait = timeout_ms + 100;
    while (g_spi->dio1_read() == 0 && wait > 0) {
        g_spi->delay_ms(1);
        wait--;
    }

    if (wait == 0) {
        g_state = SS_RADIO_IDLE;
        return 0; /* timeout */
    }

    /* Get RX buffer status: offset + length */
    g_spi->cs_select();
    g_spi->transfer(SX1262_CMD_GET_RX_BUFFER);
    g_spi->transfer(0x00);
    uint8_t rx_offset = g_spi->transfer(0x00);
    uint8_t rx_len = g_spi->transfer(0x00);
    g_spi->cs_release();

    if (rx_len > max_len) rx_len = max_len;

    /* Read received data */
    read_buffer(rx_offset, buf, rx_len);

    /* Read RSSI from register (simplified) */
    g_last_rssi = (int8_t)read_reg(0x0267) - 157; /* Approximate */

    /* Clear IRQ */
    g_spi->cs_select();
    g_spi->transfer(SX1262_CMD_CLEAR_IRQ);
    g_spi->transfer(0xFF);
    g_spi->transfer(0xFF);
    g_spi->cs_release();

    g_state = SS_RADIO_IDLE;
    return rx_len;
}

int8_t ss_radio_get_rssi(void)
{
    return g_last_rssi;
}

void ss_radio_sleep(void)
{
    if (!g_spi) return;
    g_spi->cs_select();
    g_spi->transfer(SX1262_CMD_SET_SLEEP);
    g_spi->transfer(0x04); /* Warm start, RTC enabled */
    g_spi->cs_release();
    g_state = SS_RADIO_SLEEP;
}

int ss_radio_wakeup(void)
{
    if (!g_spi) return -1;
    /* Toggle CS to wake (SX1262 wakes on NSS falling edge) */
    g_spi->cs_select();
    g_spi->delay_ms(1);
    g_spi->cs_release();
    g_spi->delay_ms(5);
    set_standby();
    return 0;
}

ss_radio_state_t ss_radio_get_state(void)
{
    return g_state;
}