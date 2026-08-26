#ifndef MOBILITYSYNC_MESH_H
#define MOBILITYSYNC_MESH_H

#include <stdint.h>

#include "protocol.h"

typedef struct {
    uint16_t node_id;
    uint8_t slot_index;
    uint16_t uplink_period_ms;
    uint8_t retries;
} mb_mesh_config_t;

void mb_mesh_init(const mb_mesh_config_t *config);
void mb_mesh_send(const mb_frame_t *frame);
void mb_mesh_log_metric(const char *label, float value);

#endif
