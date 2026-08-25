#include "mesh.h"

#include <stdio.h>

static ms_mesh_config_t g_config;

void ms_mesh_init(const ms_mesh_config_t *config) {
    if (config) {
        g_config = *config;
    }
    printf("[mesh] init node=%u slot=%u period=%lu retries=%u\n",
           (unsigned)g_config.node_id,
           (unsigned)g_config.slot_index,
           (unsigned long)g_config.uplink_period_ms,
           (unsigned)g_config.retries);
}

int ms_mesh_send(const ms_frame_t *frame) {
    if (!frame) {
        return -1;
    }
    printf("[mesh] send src=%u dst=%u type=%u bytes=%u\n",
           (unsigned)frame->source_id,
           (unsigned)frame->destination_id,
           (unsigned)frame->message_type,
           (unsigned)frame->payload_length);
    return 0;
}
