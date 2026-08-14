#ifndef TDMA_COORDINATOR_H
#define TDMA_COORDINATOR_H

#include <stdint.h>
#include "../common/protocol.h"
#include "../common/mesh.h"

/* SX1262 handle stub — defined in hub main.c */
typedef struct {
    int cs_pin;
    int dio1_pin;
    int busy_pin;
    int reset_pin;
} sx1262_handle_t;

void sx1262_tx(sx1262_handle_t *radio, const uint8_t *data, uint16_t len);

void tdma_send_beacon(mesh_state_t *state, sx1262_handle_t *radio, uint32_t timestamp);
void tdma_handle_join(mesh_state_t *state, sx1262_handle_t *radio, const postsync_frame_t *req);
void tdma_broadcast_config(mesh_state_t *state, sx1262_handle_t *radio,
                            uint8_t target_node, const uint8_t *config, uint8_t len);
void tdma_send_alert(mesh_state_t *state, sx1262_handle_t *radio,
                     uint16_t target, const posture_alert_t *alert);

#endif