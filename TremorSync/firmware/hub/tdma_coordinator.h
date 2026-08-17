#ifndef TREMORSYNC_TDMA_COORDINATOR_H
#define TREMORSYNC_TDMA_COORDINATOR_H

#include <stdint.h>
#include <stdbool.h>
#include "../common/mesh.h"

typedef struct {
    mesh_state_t  mesh;
    uint32_t      superframe_count;
    uint32_t      last_beacon_ms;
} tdma_state_t;

void     tdma_init(tdma_state_t *s, uint16_t self_id);
uint32_t tdma_slot_start_ms(tdma_state_t *s, uint8_t slot);
bool     tdma_is_my_slot(tdma_state_t *s, uint8_t slot, uint32_t now_ms);
void     tdma_tick(tdma_state_t *s, uint32_t now_ms,
                   void (*tx_beacon)(const mesh_frame_t *),
                   void (*tx_slot)(const mesh_frame_t *));

#endif