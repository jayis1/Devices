#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "../common/mesh.h"
#include "../common/protocol.h"

typedef struct {
    uint32_t uptime_s;
    uint8_t online_nodes;
    uint8_t advisory_state;
    uint32_t alerts_sent;
} hub_state_t;

static uint8_t derive_state(const wsync_water_quality_t *wq, const wsync_pump_state_t *pump, const wsync_weather_t *weather) {
    if (wq->turbidity_ntu_x10 > 40u || wq->orp_mv < 180 || pump->dry_run_score_pct > 85u) {
        return WSYNC_ALERT_DO_NOT_DRINK;
    }
    if (weather->rain_mm_x10 > 150u || pump->short_cycle_score_pct > 70u || wq->ph_x100 < 620) {
        return WSYNC_ALERT_TREAT;
    }
    if (pump->cavitation_score_pct > 35u || weather->freeze_risk_pct > 50u) {
        return WSYNC_ALERT_WATCH;
    }
    return WSYNC_ALERT_SAFE;
}

static const char *state_name(uint8_t state) {
    switch (state) {
        case WSYNC_ALERT_SAFE: return "safe";
        case WSYNC_ALERT_WATCH: return "watch";
        case WSYNC_ALERT_TREAT: return "treat";
        default: return "do_not_drink";
    }
}

int main(void) {
    wsync_mesh_queue_t txq;
    wsync_mesh_init(&txq);
    hub_state_t state = {0};

    wsync_water_quality_t wq = { .ph_x100 = 648, .ec_us_cm = 540, .orp_mv = 202, .turbidity_ntu_x10 = 22, .pressure_kpa = 410, .temp_c_x100 = 1134, .advisory_state = 0 };
    wsync_pump_state_t pump = { .rms_current_x100 = 824, .peak_start_current_x100 = 1420, .pressure_rise_kpa = 182, .starts_per_hour_x10 = 18, .cavitation_score_pct = 14, .short_cycle_score_pct = 21, .dry_run_score_pct = 5, .enabled = 1 };
    wsync_weather_t weather = { .rain_mm_x10 = 38, .soil_shallow_pct_x100 = 5120, .soil_mid_pct_x100 = 4480, .soil_deep_pct_x100 = 3890, .pressure_hpa_x10 = 10082, .temp_c_x100 = 1844, .freeze_risk_pct = 4, .dry_spell_days = 1 };

    state.advisory_state = derive_state(&wq, &pump, &weather);
    for (int tick = 0; tick < 5; ++tick) {
        state.uptime_s += 60u;
        state.online_nodes = 5u;
        state.alerts_sent += (state.advisory_state >= WSYNC_ALERT_TREAT) ? 1u : 0u;
        printf("hub uptime=%u nodes=%u advisory=%s alerts=%u\n",
               state.uptime_s,
               state.online_nodes,
               state_name(state.advisory_state),
               state.alerts_sent);
    }

    wsync_frame_t frame;
    wsync_build_frame(&frame, WSYNC_NODE_HUB, WSYNC_NODE_BROADCAST, WSYNC_MSG_ALERT, 1u, 0x12345678u, (const uint8_t *)&state.advisory_state, 1u);
    wsync_mesh_enqueue(&txq, &frame, 1u);

    uint8_t payload_len = 0u;
    while (wsync_mesh_dequeue(&txq, &frame, &payload_len)) {
        printf("tx frame dst=%u type=%u payload=%u crc=%04X\n", frame.dst_id, frame.msg_type, payload_len, frame.crc);
    }

    return 0;
}
