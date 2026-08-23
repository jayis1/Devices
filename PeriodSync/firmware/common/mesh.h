#ifndef PERIODSYNC_MESH_H
#define PERIODSYNC_MESH_H

#include <stdbool.h>
#include <stdint.h>
#include "protocol.h"

typedef struct {
    uint16_t node_id;
    uint8_t channel;
    int8_t tx_power_dbm;
} mesh_config_t;

void mesh_init(const mesh_config_t *config);
bool mesh_send_frame(const psync_frame_t *frame);
bool mesh_receive_frame(psync_frame_t *frame);

#endif
