/**
 * AsthmaSync — SX1262 Radio Driver Wrapper
 * ========================================
 * Thin abstraction over Semtech SX126x driver for Sub-GHz
 * TDMA mesh communication between Hub and Air Sentinel.
 *
 * The actual SX126x HAL is provided by the Semtech radio driver
 * (https://github.com/Lora-net/sx126x).  This wrapper handles:
 *   - Initialization (frequency, TX power, modulation)
 *   - TDMA slot management (Hub is coordinator)
 *   - Send / receive with AsthmaSync protocol packets
 *   - RSSI / SNR reporting
 *
 * License: MIT
 */

#ifndef ASTHMASYNC_RADIO_H
#define ASTHMASYNC_RADIO_H

#include "protocol.h"
#include <stdint.h>

/* ── Radio Configuration ────────────────────────────────── */
#define RADIO_FREQ_HZ        868000000ULL   /* EU 868 MHz band */
#define RADIO_TX_POWER_DBM   14              /* +14 dBm (25 mW) */
#define RADIO_BW_HZ          125000          /* 125 kHz */
#define RADIO_SF              7               /* Spreading factor 7 (fast) */
#define RADIO_CR              5               /* Coding rate 4/5 */
#define RADIO_PREAMBLE_LEN    8
#define RADIO_TX_TIMEOUT_MS   3000
#define RADIO_RX_TIMEOUT_MS   5000

/* ── TDMA Configuration ─────────────────────────────────── */
#define TDMA_SUPERFRAME_MS    2000    /* 2-second superframe */
#define TDMA_SLOT_MS          200     /* 200 ms per slot */
#define TDMA_GUARD_MS         20      /* guard band */
#define TDMA_MAX_NODES        8       /* up to 8 mesh nodes */
#define TDMA_BEACON_SLOT      0       /* hub beacon in slot 0 */

/* ── Radio State ────────────────────────────────────────── */
typedef enum {
    RADIO_STATE_IDLE = 0,
    RADIO_STATE_RX,
    RADIO_STATE_TX,
    RADIO_STATE_SLEEP,
} radio_state_t;

/* ── API ────────────────────────────────────────────────── */

/** Initialize SX1262 and radio state machine. Returns 0 on success. */
int radio_init(void);

/** Send a packet. Blocks until TX complete or timeout. Returns 0 on success. */
int radio_send(const uint8_t *data, size_t len);

/** Receive a packet (non-blocking). Returns packet length, 0 if none, -1 on error. */
int radio_recv(uint8_t *buf, size_t buf_size, int8_t *rssi, int8_t *snr);

/** Set radio to receive mode. */
int radio_rx_start(uint32_t timeout_ms);

/** Put radio to sleep (low power). */
int radio_sleep(void);

/** Wake radio from sleep. */
int radio_wakeup(void);

/** Get last RSSI and SNR. */
void radio_get_stats(int8_t *rssi, int8_t *snr);

/* ── TDMA (Hub side: coordinator) ───────────────────────── */

/** Hub: start TDMA superframe — send beacon, then listen per slot. */
int tdma_hub_superframe(void);

/** Hub: assign a TDMA slot to a joining node. */
int tdma_assign_slot(uint16_t node_id, uint8_t *out_slot);

/* ── TDMA (Node side: Air Sentinel) ─────────────────────── */

/** Node: synchronize to hub beacon and get assigned slot. */
int tdma_node_sync(uint32_t timeout_ms);

/** Node: wait for assigned slot and send packet. */
int tdma_node_send(const uint8_t *data, size_t len);

/** Node: get assigned slot number. */
uint8_t tdma_get_slot(void);

#endif /* ASTHMASYNC_RADIO_H */