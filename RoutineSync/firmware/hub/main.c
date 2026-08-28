#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../common/mesh.h"
#include "../common/protocol.h"

typedef struct {
    uint8_t routine_id;
    float miss_risk;
    uint8_t missing_items;
    uint16_t tray_mass_delta_g;
} departure_state_t;

static float compute_miss_risk(const rs_doorway_status_t *status) {
    float risk = 0.10f;
    risk += 0.18f * (float)status->missing_items;
    risk += status->door_open ? 0.12f : 0.0f;
    risk += (status->minutes_to_deadline < 10u) ? 0.22f : 0.05f;
    risk += (status->tray_mass_delta_g > 120u) ? 0.16f : 0.02f;
    if (risk > 0.99f) {
        risk = 0.99f;
    }
    return risk;
}

static void log_departure_summary(const departure_state_t *state) {
    printf("[hub] routine=%u missing=%u tray_delta=%ug miss_risk=%.2f\n",
           state->routine_id,
           state->missing_items,
           state->tray_mass_delta_g,
           state->miss_risk);
}

int main(void) {
    rs_mesh_ctx_t mesh;
    rs_mesh_init(&mesh, RS_NODE_HUB, 0x12345678u);

    rs_doorway_status_t doorway = {
        .routine_id = 2,
        .missing_items = 1,
        .checklist_complete_pct = 75,
        .door_open = 1,
        .tray_mass_delta_g = 146,
        .nfc_flags = 0x0001,
        .nearest_tag_range_cm = 315,
        .minutes_to_deadline = 8,
    };

    departure_state_t state;
    memset(&state, 0, sizeof(state));
    state.routine_id = doorway.routine_id;
    state.missing_items = doorway.missing_items;
    state.tray_mass_delta_g = doorway.tray_mass_delta_g;
    state.miss_risk = compute_miss_risk(&doorway);

    rs_frame_t heartbeat;
    rs_mesh_make_heartbeat(&mesh, &heartbeat, RS_NODE_BROADCAST);
    printf("[hub] heartbeat seq=%u crc=0x%04X\n", heartbeat.seq, heartbeat.crc);
    log_departure_summary(&state);

    if (state.miss_risk > 0.70f) {
        printf("[hub] action=tag_chirp detail=likely missing laptop in office\n");
    } else {
        printf("[hub] action=none detail=departure clear\n");
    }

    return 0;
}
