#include "mesh.h"

#include <stdio.h>

static mb_mesh_config_t g_config;

void mb_mesh_init(const mb_mesh_config_t *config) {
    if (config) {
        g_config = *config;
    }
    printf("[mesh] node=%u slot=%u period_ms=%u retries=%u\n",
           g_config.node_id,
           g_config.slot_index,
           g_config.uplink_period_ms,
           g_config.retries);
}

void mb_mesh_send(const mb_frame_t *frame) {
    if (!frame) {
        return;
    }
    printf("[mesh] tx type=%u src=%u dst=%u len=%u\n",
           frame->message_type,
           frame->source_id,
           frame->destination_id,
           frame->payload_length);
}

void mb_mesh_log_metric(const char *label, float value) {
    printf("[metric] %s=%.2f\n", label ? label : "unknown", value);
}
