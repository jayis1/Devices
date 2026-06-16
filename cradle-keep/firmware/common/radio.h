/**
 * CradleKeep — Common Radio Driver Interface
 * 
 * Abstracts the SX1261/SX1262 Sub-GHz LoRa radio for all nodes.
 * Each node provides platform-specific implementations.
 */

#ifndef CRADLEKEEP_RADIO_H
#define CRADLEKEEP_RADIO_H

#include <stdint.h>
#include <stdbool.h>
#include "protocol.h"

/* ── Radio Configuration ─────────────────────────────────────────── */
typedef struct {
    uint8_t  address;           /* This node's address (ADDR_*) */
    uint32_t frequency;         /* 868000000 (EU) or 915000000 (US) */
    uint8_t  spreading_factor;  /* SF7 (normal) or SF9 (alerts) */
    uint8_t  bandwidth;         /* 4 (125kHz) */
    uint8_t  coding_rate;       /* 1 (4/5) */
    uint8_t  tx_power;          /* +14 dBm (nodes), +20 dBm (hub) */
    uint16_t preamble_len;      /* 8 symbols */
    uint32_t sync_word;         /* 0x0C4B ("CK") */
} radio_config_t;

/* ── Radio Statistics ──────────────────────────────────────────────── */
typedef struct {
    uint32_t tx_count;
    uint32_t rx_count;
    uint32_t tx_errors;
    uint32_t rx_errors;
    uint32_t crc_errors;
    int8_t    last_rssi;
    int8_t    last_snr;
} radio_stats_t;

/* ── Platform-Specific Interface (must be implemented per MCU) ────── */

/** Initialize SPI bus and GPIO for radio */
int radio_platform_init(void);

/** Set radio NSS pin (active low) */
void radio_platform_set_nss(bool high);

/** Set radio reset pin */
void radio_platform_set_reset(bool high);

/** Read radio busy pin */
bool radio_platform_read_busy(void);

/** SPI transfer: write a byte and read response */
uint8_t radio_platform_spi_transfer(uint8_t byte);

/** Get current time in milliseconds */
uint32_t radio_platform_get_time_ms(void);

/** Delay for specified milliseconds */
void radio_platform_delay_ms(uint32_t ms);

/* ── High-Level Radio API ─────────────────────────────────────────── */

/** Initialize radio with given config */
int radio_init(const radio_config_t *config);

/** Send a packet (blocking, waits for TX complete) */
int radio_send(const packet_t *pkt);

/** Receive a packet (blocking, with timeout in ms, 0=non-blocking) */
int radio_receive(packet_t *pkt, uint32_t timeout_ms);

/** Switch to TX mode for sending */
int radio_set_tx_mode(void);

/** Switch to RX mode for receiving */
int radio_set_rx_mode(void);

/** Switch to sleep mode (lowest power) */
int radio_set_sleep(void);

/** Switch to standby mode */
int radio_set_standby(void);

/** Change spreading factor (for alert mode) */
int radio_set_spreading_factor(uint8_t sf);

/** Get radio statistics */
const radio_stats_t* radio_get_stats(void);

/* ── SX126x Register Definitions ───────────────────────────────────── */
#define SX126X_REG_OCP         0x08E7
#define SX126X_REG_PA_CONFIG   0x0889
#define SX126X_REG_TX_PARAMS   0x088D
#define SX126X_REG_MOD_PARAMS  0x088E

/* ── SX126x Commands ──────────────────────────────────────────────── */
#define SX126X_CMD_SET_STANDBY         0x80
#define SX126X_CMD_SET_FS              0x81
#define SX126X_CMD_SET_TX              0x83
#define SX126X_CMD_SET_RX             0x82
#define SX126X_CMD_SET_SLEEP           0x84
#define SX126X_CMD_SET_CAD             0x85
#define SX126X_CMD_SET_TX_CONTINUOUS   0x86
#define SX126X_CMD_SET_PACKET_TYPE     0x8A
#define SX126X_CMD_SET_MOD_PARAMS      0x8B
#define SX126X_CMD_SET_PACKET_PARAMS   0x8C
#define SX126X_CMD_SET_RF_FREQUENCY    0x86
#define SX126X_CMD_SET_TX_PARAMS       0x8E
#define SX126X_CMD_SET_BUFFER_BASE     0x8F
#define SX126X_CMD_WRITE_BUFFER        0x0D
#define SX126X_CMD_READ_BUFFER         0x1D
#define SX126X_CMD_GET_STATUS          0xC0
#define SX126X_CMD_GET_RX_STATUS      0x13
#define SX126X_CMD_GET_RSSI_INST       0x15
#define SX126X_CMD_SET_DIO_IRQ         0x08
#define SX126X_CMD_CLEAR_IRQ           0x02
#define SX126X_CMD_GET_IRQ             0x12
#define SX126X_CMD_CALIBRATE           0x89

/* ── IRQ Masks ────────────────────────────────────────────────────── */
#define SX126X_IRQ_TX_DONE      (1 << 0)
#define SX126X_IRQ_RX_DONE      (1 << 1)
#define SX126X_IRQ_PREAMBLE_DET (1 << 2)
#define SX126X_IRQ_SYNC_DET     (1 << 3)
#define SX126X_IRQ_HEADER_DET   (1 << 4)
#define SX126X_IRQ_CRC_ERR      (1 << 5)
#define SX126X_IRQ_CAD_DET      (1 << 6)
#define SX126X_IRQ_CAD_DONE     (1 << 7)
#define SX126X_IRQ_TIMEOUT      (1 << 8)

/* ── Packet Types ─────────────────────────────────────────────────── */
#define SX126X_PKT_LORA         0x01
#define SX126X_PKT_GFSK        0x00

/* ── Standby Modes ────────────────────────────────────────────────── */
#define SX126X_STANDBY_RC       0x00
#define SX126X_STANDBY_XOSC     0x01

#endif /* CRADLEKEEP_RADIO_H */