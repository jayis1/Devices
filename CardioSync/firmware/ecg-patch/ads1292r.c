/*
 * ads1292r.c — ADS1292R ECG AFE driver for nRF52840
 *
 * This driver handles SPI communication with the ADS1292R 24-bit
 * delta-sigma ADC, configures it for ECG measurement (250 SPS,
 * gain 12, RLD enabled), and reads continuous data.
 *
 * License: MIT
 */
#include "ads1292r.h"
#include <string.h>

/* ── SPI Pin Definitions (nRF52840) ────────────────────────── */
#define SPI_CS_PIN   3   /* P0.03 */
#define SPI_CLK_PIN  4   /* P0.04 */
#define SPI_MOSI_PIN 5   /* P0.05 */
#define SPI_MISO_PIN 6   /* P0.06 */
#define ADS_DRDY_PIN 7   /* P0.07 (interrupt) */
#define ADS_START_PIN 8  /* P0.08 */
#define ADS_RESET_PIN 9  /* P0.09 */

static uint8_t current_config1 = 0;
static bool initialized = false;

/* ── SPI Hardware Abstraction Stubs (nRF52840) ────────────── */
void spi_init(void)
{
    /* nRF52840 SPIM0 configuration:
     * - SPI mode 1 (CPOL=0, CPHA=1) for ADS1292R
     * - 2 MHz clock
     * - CS on P0.03, CLK on P0.04, MOSI on P0.05, MISO on P0.06
     * In production, use nrf_spim_* APIs from nRF Connect SDK
     */
}

void spi_cs_low(void)  { /* GPIO P0.03 = 0 */ }
void spi_cs_high(void) { /* GPIO P0.03 = 1 */ }

uint8_t spi_transfer(uint8_t tx)
{
    /* SPI full-duplex transfer: send tx, return rx */
    return 0;
}

void spi_transfer_buf(const uint8_t *tx, uint8_t *rx, size_t len)
{
    spi_cs_low();
    for (size_t i = 0; i < len; i++) {
        rx[i] = spi_transfer(tx[i]);
    }
    spi_cs_high();
}

void ads_delay_ms(uint32_t ms)
{
    /* nrf_delay_ms(ms) in production */
}

/* ── ADS1292R Register Access ──────────────────────────────── */
static uint8_t ads_read_reg(uint8_t reg)
{
    spi_cs_low();
    spi_transfer(ADS_CMD_RREG | reg);  /* RREG command + reg addr */
    spi_transfer(0x00);                 /* number of regs - 1 */
    uint8_t val = spi_transfer(0x00);   /* read data */
    spi_cs_high();
    ads_delay_ms(1);
    return val;
}

static void ads_write_reg(uint8_t reg, uint8_t val)
{
    spi_cs_low();
    spi_transfer(ADS_CMD_WREG | reg);  /* WREG command + reg addr */
    spi_transfer(0x00);                 /* number of regs - 1 */
    spi_transfer(val);                  /* write data */
    spi_cs_high();
    ads_delay_ms(1);
}

/* ── ADS1292R Initialization ───────────────────────────────── */
void ads_init(void)
{
    spi_init();

    /* Hardware reset (pulse RESET low for 2 tCLK) */
    /* GPIO P0.09 low → delay 1 ms → high */
    ads_delay_ms(2);

    /* Wait for DRDY to indicate device is ready */
    ads_delay_ms(10);

    /* Read device ID to verify communication */
    uint8_t id = ads_read_reg(ADS_REG_ID);
    /* Expected: 0x53 (ADS1292R) */
    if ((id & 0x1F) != 0x03) {
        /* ADS1292R ID = 0x03 in lower bits */
        /* Handle error: retry or report fault */
    }

    /* Stop continuous data mode before configuring */
    spi_cs_low();
    spi_transfer(ADS_CMD_SDATAC);
    spi_cs_high();
    ads_delay_ms(1);

    /* CONFIG1: High resolution, 250 SPS */
    ads_write_reg(ADS_REG_CONFIG1, ADS_CONFIG1_HR | ADS_CONFIG1_DR_250);
    current_config1 = ADS_CONFIG1_HR | ADS_CONFIG1_DR_250;

    /* CONFIG2: Internal test signal off */
    ads_write_reg(ADS_REG_CONFIG2, 0x00);

    /* CONFIG3: Internal reference, RLD enabled, bias on */
    ads_write_reg(ADS_REG_CONFIG3,
                  ADS_CONFIG3_RLD_ON | ADS_CONFIG3_REF_24V | ADS_CONFIG3_BIAS_ON);

    /* CH1SET: Gain 12, normal input (ECG Lead I) */
    ads_write_reg(ADS_REG_CH1SET, ADS_GAIN_12 | ADS_MUX_NORMAL);

    /* CH2SET: Gain 12, normal input (or short for single-channel) */
    ads_write_reg(ADS_REG_CH2SET, ADS_GAIN_12 | ADS_MUX_SHORT);

    /* RLD: RLD enabled, reference = mid-supply */
    ads_write_reg(ADS_REG_RLD, 0x02);

    /* RLD_SENS: RLD from CH1 positive and negative */
    ads_write_reg(ADS_REG_RLD_SENS, 0x0C);

    /* LOFF: Lead-off detection enabled, 4 Hz, 6 nA */
    ads_write_reg(ADS_REG_LOFF, 0x03);

    /* LOFF_SENS: Lead-off from CH1 */
    ads_write_reg(ADS_REG_LOFF_SENS, 0x01);

    initialized = true;
}

/* ── Start / Stop Conversion ──────────────────────────────── */
void ads_start(void)
{
    /* Set START pin high (or send START command) */
    spi_cs_low();
    spi_transfer(ADS_CMD_START);
    spi_cs_high();

    /* Begin continuous data read mode */
    ads_delay_ms(1);
    spi_cs_low();
    spi_transfer(ADS_CMD_RDATAC);
    spi_cs_high();
    ads_delay_ms(1);
}

void ads_stop(void)
{
    spi_cs_low();
    spi_transfer(ADS_CMD_SDATAC);
    spi_cs_high();
    ads_delay_ms(1);

    spi_cs_low();
    spi_transfer(ADS_CMD_STOP);
    spi_cs_high();
}

void ads_reset(void)
{
    spi_cs_low();
    spi_transfer(ADS_CMD_RESET);
    spi_cs_high();
    ads_delay_ms(10);
    initialized = false;
}

/* ── Read Single Sample ───────────────────────────────────── */
int ads_read_sample(ads_sample_t *sample)
{
    /* Wait for DRDY (poll or interrupt) */
    /* In production: use GPIO interrupt on P0.07 (ADS_DRDY_PIN) */

    uint8_t rx[9]; /* 3 status + 3 ch1 + 3 ch2 */

    spi_cs_low();
    /* Read 9 bytes: 3 status + 3 ch1 (24-bit) + 3 ch2 (24-bit) */
    uint8_t tx[9] = {0};
    spi_transfer_buf(tx, rx, 9);
    spi_cs_high();

    /* Extract lead-off status */
    sample->lead_off = rx[0] & 0x03;  /* bits 0-1: CH1 lead-off */

    /* Convert 24-bit signed to 32-bit signed (Channel 1) */
    int32_t ch1_raw = ((int32_t)rx[3] << 16) | ((int32_t)rx[4] << 8) | rx[5];
    if (ch1_raw & 0x800000) ch1_raw |= 0xFF000000;  /* sign extend */
    sample->ch1 = ch1_raw;

    /* Channel 2 */
    int32_t ch2_raw = ((int32_t)rx[6] << 16) | ((int32_t)rx[7] << 8) | rx[8];
    if (ch2_raw & 0x800000) ch2_raw |= 0xFF000000;
    sample->ch2 = ch2_raw;

    return 0;
}

/* ── Read Continuous Data ─────────────────────────────────── */
void ads_read_data_continuous(ads_sample_t *buf, int count)
{
    for (int i = 0; i < count; i++) {
        ads_read_sample(&buf[i]);
    }
}

/* ── RLD Control ──────────────────────────────────────────── */
void ads_set_rld(bool enabled)
{
    uint8_t config3 = ads_read_reg(ADS_REG_CONFIG3);
    if (enabled)
        config3 |= ADS_CONFIG3_RLD_ON;
    else
        config3 &= ~ADS_CONFIG3_RLD_ON;
    ads_write_reg(ADS_REG_CONFIG3, config3);
}

/* ── Lead-Off Check ───────────────────────────────────────── */
bool ads_check_lead_off(void)
{
    uint8_t status = ads_read_reg(ADS_REG_LOFF_STAT);
    return (status & 0x01) != 0;  /* CH1 lead-off bit */
}