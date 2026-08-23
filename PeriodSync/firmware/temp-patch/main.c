#include <stdio.h>
#include <math.h>
#include "../common/mesh.h"
#include "../common/protocol.h"

static void read_temp_patch(float *skin_temp_c, float *hr_bpm, float *hrv_rmssd) {
    *skin_temp_c = 36.72f;
    *hr_bpm = 63.4f;
    *hrv_rmssd = 42.1f;
}

int main(void) {
    mesh_config_t cfg = { .node_id = 0x2001, .channel = 3, .tx_power_dbm = 10 };
    mesh_init(&cfg);

    float skin_temp_c, hr_bpm, hrv_rmssd;
    read_temp_patch(&skin_temp_c, &hr_bpm, &hrv_rmssd);

    uint8_t payload[8] = {
        (uint8_t)((int)(skin_temp_c * 100) >> 8),
        (uint8_t)((int)(skin_temp_c * 100) & 0xFF),
        (uint8_t)((int)(hr_bpm * 10) >> 8),
        (uint8_t)((int)(hr_bpm * 10) & 0xFF),
        (uint8_t)((int)(hrv_rmssd * 10) >> 8),
        (uint8_t)((int)(hrv_rmssd * 10) & 0xFF),
        1,
        0
    };

    psync_frame_t frame;
    psync_build_frame(&frame, PSYNC_MSG_TELEMETRY, cfg.node_id, 0x1001, 1724390400UL, 0, payload, sizeof(payload));
    mesh_send_frame(&frame);
    printf("temp_patch %.2fC %.1fbpm %.1fms\n", skin_temp_c, hr_bpm, hrv_rmssd);
    return 0;
}
