#ifndef FOOD_ALLERGY_SYNC_MESH_H
#define FOOD_ALLERGY_SYNC_MESH_H

#include "protocol.h"
#include <stdint.h>

typedef struct {
    uint16_t node_id;
    uint8_t slot_index;
    uint32_t uplink_period_ms;
    uint8_t retries;
} fas_mesh_config_t;

void fas_mesh_init(const fas_mesh_config_t *config);
int fas_mesh_send(const fas_frame_t *frame);
int fas_mesh_poll(fas_frame_t *frame);

#endif
