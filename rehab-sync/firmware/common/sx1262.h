/*
 * RehabSync — SX1262 Sub-GHz Radio Driver Header
 * LoRa modulation, 868 MHz, for Hub↔PressureMat and mesh relay.
 */
#ifndef REHABSYNC_SX1262_H
#define REHABSYNC_SX1262_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* SX1262 register addresses (subset) */
#define SX1262_REG_PKT_STATUS   0x08C7
#define SX1262_REG_SYNC_WORD    0x0740
#define SX1262_REG_LORA_SYNC    0x0744
#define SX1262_REG_PA_CONFIG    0x0889
#define SX1262_REG_RX_GAIN      0x08AC

/* Operating modes */
enum sx1262_mode {
    SX1262_MODE_SLEEP    = 0x00,
    SX1262_MODE_STBY_RC  = 0x01,
    SX1262_MODE_STBY_XOSC = 0x02,
    SX1262_MODE_FS       = 0x03,
    SX1262_MODE_TX       = 0x04,
    SX1262_MODE_RX       = 0x05,
};

/* LoRa parameters */
typedef struct {
    uint32_t frequency;     /* Hz, e.g. 868000000 */
    uint8_t  spreading_factor; /* 7-12 */
    uint32_t bandwidth;     /* Hz, e.g. 250000 */
    uint8_t  coding_rate;   /* 4/5 to 4/8 (1-4) */
    int8_t   tx_power_dbm;  /* -9 to +22 */
    uint16_t preamble_len;  /* symbols */
} sx1262_config_t;

/* Radio state */
typedef struct {
    sx1262_config_t cfg;
    uint8_t  current_mode;
    int8_t   rssi;
    int8_t   snr;
    uint16_t tx_count;
    uint16_t rx_count;
    uint16_t crc_errors;
} sx1262_t;

/* API */
int  sx1262_init(sx1262_t *radio, const sx1262_config_t *cfg);
int  sx1262_set_mode(sx1262_t *radio, enum sx1262_mode mode);
int  sx1262_tx(sx1262_t *radio, const uint8_t *data, size_t len, uint32_t timeout_ms);
int  sx1262_rx(sx1262_t *radio, uint8_t *data, size_t cap, uint32_t timeout_ms);
int  sx1262_cad(sx1262_t *radio);  /* Channel Activity Detection */
void sx1262_sleep(sx1262_t *radio);

/* SPI interface (platform-specific, implemented per node) */
int  sx1262_spi_write(sx1262_t *radio, uint8_t cmd, const uint8_t *data, size_t len);
int  sx1262_spi_read(sx1262_t *radio, uint8_t cmd, uint8_t *data, size_t len);
void sx1262_reset(sx1262_t *radio);
void sx1262_dio1_isr(void);

#endif /* REHABSYNC_SX1262_H */