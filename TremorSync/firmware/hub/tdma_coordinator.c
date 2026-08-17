/*
 * TremorSync Hub — TDMA coordinator helpers
 */
#include "tdma_coordinator.h"
#include "../common/mesh.h"
#include "esp_timer.h"
#include <string.h>

void tdma_init(tdma_state_t *s, uint16_t self_id)
{
    mesh_init(&s->mesh, self_id, true);
    s->superframe_count = 0;
    s->last_beacon_ms   = 0;
}

uint32_t tdma_slot_start_ms(tdma_state_t *s, uint8_t slot)
{
    return s->last_beacon_ms + TDMA_BEACON_MS + (uint32_t)slot * TDMA_SLOT_MS;
}

bool tdma_is_my_slot(tdma_state_t *s, uint8_t slot, uint32_t now_ms)
{
    uint32_t start = tdma_slot_start_ms(s, slot);
    return (now_ms >= start && now_ms < start + TDMA_SLOT_MS);
}

void tdma_tick(tdma_state_t *s, uint32_t now_ms,
               void (*tx_beacon)(const mesh_frame_t *),
               void (*tx_slot)(const mesh_frame_t *))
{
    if (now_ms - s->last_beacon_ms >= TDMA_SUPERFRAME_MS) {
        s->last_beacon_ms = now_ms;
        s->superframe_count++;
        mesh_frame_t beacon;
        mesh_build_beacon(&s->mesh, &beacon);
        if (tx_beacon) tx_beacon(&beacon);
    }
}