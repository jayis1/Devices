#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "../common/protocol.h"

static uint8_t cavitation_score(uint16_t rms_current_x100, uint16_t pressure_rise_kpa) {
    if (pressure_rise_kpa == 0u) return 100u;
    int score = (int)(rms_current_x100 / 20u) - (int)(pressure_rise_kpa / 8u);
    if (score < 0) score = 0;
    if (score > 100) score = 100;
    return (uint8_t)score;
}

int main(void) {
    wsync_pump_state_t state = {
        .rms_current_x100 = 860,
        .peak_start_current_x100 = 1480,
        .pressure_rise_kpa = 156,
        .starts_per_hour_x10 = 24,
        .cavitation_score_pct = 0,
        .short_cycle_score_pct = 34,
        .dry_run_score_pct = 11,
        .enabled = 1,
    };
    state.cavitation_score_pct = cavitation_score(state.rms_current_x100, state.pressure_rise_kpa);
    bool shutdown = state.dry_run_score_pct > 80u || state.pressure_rise_kpa < 60u;
    printf("pump enabled=%u rms=%.2fA rise=%ukPa cavitation=%u shortcycle=%u shutdown=%u\n",
           state.enabled,
           state.rms_current_x100 / 100.0,
           state.pressure_rise_kpa,
           state.cavitation_score_pct,
           state.short_cycle_score_pct,
           shutdown ? 1u : 0u);
    return 0;
}
