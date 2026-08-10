/*
 * FireSync — Sub-GHz 868 MHz TDMA Mesh Network Layer
 * SX1262 radio driver + TDMA slot management + self-healing mesh relay
 *
 * This is a platform-abstraction header. The implementation is
 * platform-specific (ESP-IDF for ESP32-S3, HAL for STM32G431).
 */
#ifndef FIRESYNC_SUBGHZ_MESH_H
#define FIRESYNC_SUBGHZ_MESH_H

#include <stdint.h>
#include <stddef.h>
#include "protocol.h"

/* === SX1262 Radio Interface === */

/* Radio configuration */
typedef struct {
    uint32_t freq_hz;
    uint8_t  spreading_factor;
    uint32_t bandwidth_hz;
    int8_t   tx_power_dbm;
    uint16_t sync_word;
    uint8_t  preamble_len;
} fs_radio_config_t;

/* Radio state */
typedef struct {
    uint8_t  node_id;
    uint8_t  node_type;
    uint8_t  tdma_slot;       /* Assigned TDMA slot (0=Hub) */
    uint16_t msg_counter;
    uint8_t  aes_key[FS_AES_KEY_LEN];
    uint8_t  aes_nonce[FS_AES_CTR_NONCE_LEN];
    uint8_t  joined;           /* 1=joined network, 0=not joined */
    int8_t   last_rssi;
    uint8_t  battery_v;       /* ×0.01V */
} fs_mesh_ctx_t;

/* === Radio Hardware Abstraction (platform-specific) === */
/* These must be implemented per-platform (ESP-IDF or STM32 HAL) */

typedef struct {
    /* SPI interface */
    int  (*spi_init)(void);
    int  (*spi_xfer)(const uint8_t *tx, uint8_t *rx, size_t len);
    /* GPIO control */
    void (*cs_low)(void);
    void (*cs_high)(void);
    void (*reset)(int assert);
    int  (*dio1_read)(void);
    int  (*busy_read)(void);
    /* Timing */
    void (*delay_ms)(uint32_t ms);
    void (*delay_us)(uint32_t us);
    /* Interrupt callback for DIO1 rising edge */
    void (*on_dio1)(void);
} fs_radio_hal_t;

/* === SX1262 Driver Functions === */
int  fs_sx1262_init(const fs_radio_hal_t *hal, const fs_radio_config_t *cfg);
int  fs_sx1262_tx(const fs_radio_hal_t *hal, const uint8_t *data, size_t len,
                  int8_t power_dbm);
int  fs_sx1262_rx(fs_radio_hal_t *hal, uint8_t *buf, size_t max_len,
                 uint32_t timeout_ms, int8_t *rssi);
int  fs_sx1262_sleep(const fs_radio_hal_t *hal);
int  fs_sx1262_set_freq(const fs_radio_hal_t *hal, uint32_t freq_hz);
void fs_sx1262_irq_handler(const fs_radio_hal_t *hal);

/* === TDMA Mesh Layer === */
int  fs_mesh_init(fs_mesh_ctx_t *ctx, uint8_t node_id, uint8_t node_type,
                  const uint8_t *aes_key);
int  fs_mesh_join(fs_mesh_ctx_t *ctx, const fs_radio_hal_t *hal);
int  fs_mesh_send(fs_mesh_ctx_t *ctx, const fs_radio_hal_t *hal,
                  const fs_message_t *msg, uint8_t priority);
int  fs_mesh_recv(fs_mesh_ctx_t *ctx, const fs_radio_hal_t *hal,
                  fs_message_t *msg, uint32_t timeout_ms);
int  fs_mesh_relay(fs_mesh_ctx_t *ctx, const fs_radio_hal_t *hal,
                   const fs_message_t *msg);
int  fs_mesh_aes_encrypt(fs_mesh_ctx_t *ctx, uint8_t *data, size_t len,
                         const uint8_t *nonce);
int  fs_mesh_aes_decrypt(fs_mesh_ctx_t *ctx, uint8_t *data, size_t len,
                         const uint8_t *nonce);

/* === TDMA Slot Management === */
/* Hub: coordinator, assigns slots to nodes.
 * Nodes: transmit in assigned slot, relay in other slots if needed.
 * Priority slot (15): emergency FIRE_ALERT bypasses TDMA (3× immediate TX).
 */

typedef struct {
    uint8_t  slot_id;        /* TDMA slot (0-16) */
    uint8_t  node_id;        /* Node assigned to this slot */
    uint8_t  active;          /* 1=slot in use */
    uint32_t last_heard_ms;  /* Last time we heard from this node */
} fs_tdma_slot_t;

#define FS_TDMA_SLOT_TABLE_SIZE  FS_TDMA_SLOTS

/* === Mesh Topology (Hub maintains) === */
typedef struct {
    uint8_t  node_id;
    uint8_t  node_type;
    uint8_t  tdma_slot;
    int8_t   rssi;
    uint8_t  battery_v;
    uint8_t  online;
    uint32_t last_seen_ms;
} fs_node_info_t;

#define FS_MESH_MAX_NODES  FS_MAX_NODES

/* Hub: broadcast FIRE_CONFIRM to all nodes */
int fs_mesh_broadcast_emergency(fs_mesh_ctx_t *ctx, const fs_radio_hal_t *hal,
                                const fs_message_t *msg);

/* Node: send emergency FIRE_ALERT (bypasses TDMA, 3× immediate TX) */
int fs_mesh_send_emergency(fs_mesh_ctx_t *ctx, const fs_radio_hal_t *hal,
                           const fs_message_t *msg);

/* Hub: send with ACK (3 retries, 500 ms timeout) */
int fs_mesh_send_acked(fs_mesh_ctx_t *ctx, const fs_radio_hal_t *hal,
                       const fs_message_t *msg, uint32_t timeout_ms,
                       uint8_t max_retries);

#endif /* FIRESYNC_SUBGHZ_MESH_H */