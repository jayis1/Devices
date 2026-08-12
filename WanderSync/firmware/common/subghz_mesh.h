/*
 * WanderSync — Sub-GHz 868 MHz TDMA Mesh Network Layer
 * SX1262 radio driver + TDMA slot management + self-healing mesh relay
 */
#ifndef WANDERSYNC_SUBGHZ_MESH_H
#define WANDERSYNC_SUBGHZ_MESH_H

#include <stdint.h>
#include <stddef.h>
#include "protocol.h"

/* === SX1262 Radio Interface === */

typedef struct {
    uint32_t freq_hz;
    uint8_t  spreading_factor;
    uint32_t bandwidth_hz;
    int8_t   tx_power_dbm;
    uint16_t sync_word;
    uint8_t  preamble_len;
} ws_radio_config_t;

typedef struct {
    uint8_t  node_id;
    uint8_t  node_type;
    uint8_t  tdma_slot;
    uint16_t msg_counter;
    uint8_t  aes_key[WS_AES_KEY_LEN];
    uint8_t  aes_nonce[WS_AES_CTR_NONCE_LEN];
    uint8_t  joined;
    int8_t   last_rssi;
    uint8_t  battery_v;
} ws_mesh_ctx_t;

/* === Radio Hardware Abstraction === */
typedef struct {
    int  (*spi_init)(void);
    int  (*spi_xfer)(const uint8_t *tx, uint8_t *rx, size_t len);
    void (*cs_low)(void);
    void (*cs_high)(void);
    void (*reset)(int assert);
    int  (*dio1_read)(void);
    int  (*busy_read)(void);
    void (*delay_ms)(uint32_t ms);
    void (*delay_us)(uint32_t us);
    void (*on_dio1)(void);
} ws_radio_hal_t;

/* === SX1262 Driver Functions === */
int  ws_sx1262_init(const ws_radio_hal_t *hal, const ws_radio_config_t *cfg);
int  ws_sx1262_tx(const ws_radio_hal_t *hal, const uint8_t *data, size_t len,
                  int8_t power_dbm);
int  ws_sx1262_rx(ws_radio_hal_t *hal, uint8_t *buf, size_t max_len,
                 uint32_t timeout_ms, int8_t *rssi);
int  ws_sx1262_sleep(const ws_radio_hal_t *hal);
int  ws_sx1262_set_freq(const ws_radio_hal_t *hal, uint32_t freq_hz);
void ws_sx1262_irq_handler(const ws_radio_hal_t *hal);

/* === TDMA Mesh Layer === */
int  ws_mesh_init(ws_mesh_ctx_t *ctx, uint8_t node_id, uint8_t node_type,
                  const uint8_t *aes_key);
int  ws_mesh_join(ws_mesh_ctx_t *ctx, const ws_radio_hal_t *hal);
int  ws_mesh_send(ws_mesh_ctx_t *ctx, const ws_radio_hal_t *hal,
                  const ws_message_t *msg, uint8_t priority);
int  ws_mesh_recv(ws_mesh_ctx_t *ctx, const ws_radio_hal_t *hal,
                  ws_message_t *msg, uint32_t timeout_ms);
int  ws_mesh_relay(ws_mesh_ctx_t *ctx, const ws_radio_hal_t *hal,
                   const ws_message_t *msg);
int  ws_mesh_aes_encrypt(ws_mesh_ctx_t *ctx, uint8_t *data, size_t len,
                         const uint8_t *nonce);
int  ws_mesh_aes_decrypt(ws_mesh_ctx_t *ctx, uint8_t *data, size_t len,
                         const uint8_t *nonce);

/* === Mesh Topology === */
typedef struct {
    uint8_t  node_id;
    uint8_t  node_type;
    uint8_t  tdma_slot;
    int8_t   rssi;
    uint8_t  battery_v;
    uint8_t  online;
    uint32_t last_seen_ms;
} ws_node_info_t;

#define WS_MESH_MAX_NODES  WS_MAX_NODES

/* Hub: broadcast emergency to all nodes */
int ws_mesh_broadcast_emergency(ws_mesh_ctx_t *ctx, const ws_radio_hal_t *hal,
                                const ws_message_t *msg);

/* Node: send emergency alert (bypasses TDMA, 3× immediate TX) */
int ws_mesh_send_emergency(ws_mesh_ctx_t *ctx, const ws_radio_hal_t *hal,
                           const ws_message_t *msg);

/* Hub: send with ACK (3 retries, 500 ms timeout) */
int ws_mesh_send_acked(ws_mesh_ctx_t *ctx, const ws_radio_hal_t *hal,
                       const ws_message_t *msg, uint32_t timeout_ms,
                       uint8_t max_retries);

#endif /* WANDERSYNC_SUBGHZ_MESH_H */