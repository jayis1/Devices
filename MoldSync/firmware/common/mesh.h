#ifndef MOLDSYNC_MESH_H
#define MOLDSYNC_MESH_H

#include "protocol.h"

typedef struct {
    uint16_t node_id;
    uint8_t slot_index;
    uint32_t uplink_period_ms;
    uint8_t retries;
} ms_mesh_config_t;

void ms_mesh_init(const ms_mesh_config_t *config);
int ms_mesh_send(const ms_frame_t *frame);

#endif
