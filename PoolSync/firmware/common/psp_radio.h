/**
 * @file psp_radio.h
 * @brief PoolSync SX1262 Sub-GHz radio driver abstraction
 */

#ifndef PSP_RADIO_H
#define PSP_RADIO_H

#include <stdint.h>
#include <stdbool.h>
#include "psp_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * RADIO CONFIGURATION
 * ============================================================ */

#define PSP_RADIO_FREQ_HZ       868000000   /* 868 MHz ISM band */
#define PSP_RADIO_TX_POWER_DBM  14          /* +14 dBm (25 mW) */
#define PSP_RADIO_BW_KHZ        125         /* LoRa bandwidth */
#define PSP_RADIO_SF            9           /* Spreading factor 9 */
#define PSP_RADIO_CR            4           /* Coding rate 4/5 */
#define PSP_RADIO_PREAMBLE_LEN  8          /* Symbols */
#define PSP_RADIO_SYNC_WORD     0x12        /* LoRa sync word (private) */

/* TDMA time slots (ms) — hub is time master */
#define PSP_RADIO_SLOT_DURATION    100     /* ms per slot */
#define PSP_RADIO_SLOTS_PER_FRAME  10      /* 10 slots per TDMA frame */
#define PSP_RADIO_FRAME_DURATION   1000     /* ms per TDMA frame = 1 second */

/* Slot assignments within a frame */
#define PSP_RADIO_SLOT_HUB_TX      0       /* Hub broadcasts */
#define PSP_RADIO_SLOT_HUB_RX      1       /* Hub listens */
#define PSP_RADIO_SLOT_PROBE1      2       /* Chemistry Probe 1 */
#define PSP_RADIO_SLOT_PROBE2      3       /* Chemistry Probe 2 */
#define PSP_RADIO_SLOT_PROBE3      4       /* Chemistry Probe 3 */
#define PSP_RADIO_SLOT_CAMERA      5       /* Pool Camera */
#define PSP_RADIO_SLOT_EQUIP       6       /* Equipment Controller */
#define PSP_RADIO_SLOT_SOLAR       7       /* Solar Monitor */
#define PSP_RADIO_SLOT_ALARM       8       /* Any node (contention) */
#define PSP_RADIO_SLOT_FREE        9       /* Unassigned */

/* ============================================================
 * RADIO API
 * ============================================================ */

/**
 * Initialize SX1262 radio
 * Configures SPI, sets frequency, modulation, power
 */
int psp_radio_init(void);

/**
 * Send a PSP frame over Sub-GHz radio
 * Uses TDMA slot timing for collision avoidance
 * @param frame  Frame to transmit
 * @return 0 on success, negative on error
 */
int psp_radio_send(const psp_frame_t *frame);

/**
 * Receive a PSP frame from Sub-GHz radio (blocking)
 * @param frame  Output frame
 * @param timeout_ms  Maximum wait time
 * @return 0 on success, -1 on timeout, negative on error
 */
int psp_radio_recv(psp_frame_t *frame, uint32_t timeout_ms);

/**
 * Send a PSP frame in the alarm slot (contention-based)
 * Used for urgent safety alarms that can't wait for assigned slot
 */
int psp_radio_send_alarm(const psp_frame_t *frame);

/**
 * Get RSSI of last received packet
 */
int8_t psp_radio_get_rssi(void);

/**
 * Get SNR of last received packet
 */
int8_t psp_radio_get_snr(void);

/**
 * Put radio to sleep (low-power mode for battery nodes)
 */
void psp_radio_sleep(void);

/**
 * Wake radio from sleep
 */
int psp_radio_wake(void);

/**
 * Set TX power (0–22 dBm)
 */
int psp_radio_set_power(int8_t dbm);

/**
 * Channel activity detection — check if channel is clear before TX
 * @return true if channel is clear
 */
bool psp_radio_channel_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* PSP_RADIO_H */