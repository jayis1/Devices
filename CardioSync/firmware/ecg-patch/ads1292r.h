/*
 * ads1292r.h — ADS1292R ECG AFE register definitions and driver API
 *
 * The ADS1292R is a 24-bit, delta-sigma ADC designed for biopotential
 * measurements. It has 2 channels, integrated PGA, and a right-leg
 * drive (RLD) amplifier for common-mode noise cancellation.
 *
 * License: MIT
 */
#ifndef ADS1292R_H
#define ADS1292R_H

#include <stdint.h>
#include <stdbool.h>

/* ── SPI Commands ───────────────────────────────────────────── */
#define ADS_CMD_WAKEUP      0x00
#define ADS_CMD_STANDBY     0x04
#define ADS_CMD_RESET       0x06
#define ADS_CMD_START       0x08
#define ADS_CMD_STOP        0x0A
#define ADS_CMD_OFFSETCAL   0x1A
#define ADS_CMD_RDATAC      0x10
#define ADS_CMD_SDATAC      0x11
#define ADS_CMD_RDATA       0x12
#define ADS_CMD_RREG        0x20  /* + register address */
#define ADS_CMD_WREG        0x40  /* + register address */

/* ── Register Map ────────────────────────────────────────────── */
#define ADS_REG_CONFIG1     0x00
#define ADS_REG_CONFIG2     0x01
#define ADS_REG_LOFF        0x02
#define ADS_REG_CH1SET      0x03
#define ADS_REG_CH2SET      0x04
#define ADS_REG_RLD         0x05
#define ADS_REG_RLD_SENS    0x06
#define ADS_REG_LOFF_SENS   0x07
#define ADS_REG_LOFF_FLIP   0x08
#define ADS_REG_RESP1       0x09
#define ADS_REG_RESP2       0x0A
#define ADS_REG_CONFIG3     0x0B
#define ADS_REG_BIAS_SENSP  0x0C
#define ADS_REG_BIAS_SENSN  0x0D
#define ADS_REG_LOFF_STAT   0x0E
#define ADS_REG_GPIO        0x0F
#define ADS_REG_ID          0x11

/* ── CONFIG1 settings ───────────────────────────────────────── */
#define ADS_CONFIG1_HR      (1 << 7)  /* High resolution mode (2.048 kHz) */
#define ADS_CONFIG1_DR_250  0x00       /* 250 SPS in HR mode */
#define ADS_CONFIG1_DR_500  0x01       /* 500 SPS in HR mode */
#define ADS_CONFIG1_DR_1K   0x02       /* 1 kSPS in HR mode */

/* ── CH1SET settings ────────────────────────────────────────── */
#define ADS_GAIN_12         0x60       /* PGA gain 12 (default for ECG) */
#define ADS_MUX_NORMAL      0x00       /* Normal input */
#define ADS_MUX_SHORT       0x01       /* Shorted input (offset test) */
#define ADS_MUX_RLD_MEAS    0x05       /* RLD measurement */
#define ADS_MUX_OPEN        0x07       /* Input open (lead-off detect) */

/* ── CONFIG3 settings ───────────────────────────────────────── */
#define ADS_CONFIG3_RLD_ON  (1 << 7)
#define ADS_CONFIG3_REF_24V (1 << 5)  /* 2.4V internal reference */
#define ADS_CONFIG3_BIAS_ON (1 << 4)  /* Internal bias enabled */

/* ── Data packet ────────────────────────────────────────────── */
#define ADS_CHANNELS        2
#define ADS_SAMPLE_BYTES    (3 * ADS_CHANNELS) /* 24-bit × 2 channels */

typedef struct {
    int32_t ch1;    /* Channel 1 (ECG) — 24-bit signed, left-aligned to 32 */
    int32_t ch2;    /* Channel 2 (ECG or respiration) */
    uint8_t lead_off; /* Lead-off status byte */
} ads_sample_t;

/* ── Driver API ──────────────────────────────────────────────── */
void ads_init(void);
void ads_start(void);
void ads_stop(void);
void ads_reset(void);
int  ads_read_sample(ads_sample_t *sample);
void ads_read_data_continuous(ads_sample_t *buf, int count);
void ads_set_rld(bool enabled);
bool ads_check_lead_off(void);

/* ── SPI Hardware Abstraction (nRF52840) ───────────────────── */
void spi_init(void);
void spi_cs_low(void);
void spi_cs_high(void);
uint8_t spi_transfer(uint8_t tx);
void spi_transfer_buf(const uint8_t *tx, uint8_t *rx, size_t len);
void ads_delay_ms(uint32_t ms);

#endif /* ADS1292R_H */