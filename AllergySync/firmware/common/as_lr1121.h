/*
 * AllergySync — LR1121 Sub-GHz Radio Driver (Platform-Abstract)
 * Shared radio interface used by all mesh nodes.
 * Platform-specific SPI glue provided by each node's port.
 *
 * LR1121: Semtech long-range transceiver, 868 MHz, +22 dBm, LoRa/FSK.
 * AllergySync uses FSK with custom TDMA MAC (not LoRa modulation) for
 * deterministic latency and higher throughput (50 kbps, 12.5 kHz BW).
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef AS_LR1121_H
#define AS_LR1121_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ---- Radio state ---- */
typedef enum {
    AS_RADIO_SLEEP = 0,
    AS_RADIO_STDBY,
    AS_RADIO_FS_TX,
    AS_RADIO_TX,
    AS_RADIO_FS_RX,
    AS_RADIO_RX,
} as_radio_state_t;

/* ---- TX/RX result ---- */
typedef enum {
    AS_RADIO_OK = 0,
    AS_RADIO_TIMEOUT = -1,
    AS_RADIO_CRC_ERR = -2,
    AS_RADIO_SPI_ERR = -3,
    AS_RADIO_BUSY = -4,
} as_radio_result_t;

/* ---- Packet info ---- */
typedef struct {
    int8_t   rssi;       /* dBm */
    int8_t   snr;        /* dB × 4 (Q5.3) */
    uint16_t length;     /* payload bytes */
} as_radio_pkt_info_t;

/* ---- Platform SPI interface (implemented per node) ---- */
typedef struct {
    void (*cs_select)(void);
    void (*cs_release)(void);
    void (*spi_xfer)(const uint8_t *tx, uint8_t *rx, size_t len);
    void (*reset)(bool assert);
    bool (*busy_read)(void);
    void (*delay_ms)(uint32_t ms);
} as_lr1121_port_t;

/* ---- API ---- */
int  as_lr1121_init(const as_lr1121_port_t *port);
void as_lr1121_set_channel(uint32_t freq_hz);
void as_lr1121_set_tx_power(int8_t dbm);
void as_lr1121_set_modem_fsk(uint32_t br_bps, uint32_t fdev_hz,
                             uint32_t bw_hz);
void as_lr1121_set_sync_word(const uint8_t *sync, uint8_t len);

/*
 * Transmit a packet (blocking, with timeout).
 * Returns AS_RADIO_OK on success.
 */
int as_lr1121_tx(const uint8_t *data, size_t len, uint32_t timeout_ms);

/*
 * Receive a packet (blocking, with timeout).
 * buf must be at least 256 bytes. Returns AS_RADIO_OK on success.
 */
int as_lr1121_rx(uint8_t *buf, as_radio_pkt_info_t *info,
                 uint32_t timeout_ms);

void as_lr1121_sleep(void);
void as_lr1121_standby(void);

/* IRQ callback type */
typedef void (*as_radio_irq_cb)(as_radio_pkt_info_t *info);

#endif /* AS_LR1121_H */