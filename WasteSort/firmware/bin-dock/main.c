#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../common/protocol.h"

typedef struct {
    uint16_t distance_mm;
    uint16_t mass_grams;
    uint16_t voc_index;
    int16_t temp_c_x100;
    uint16_t rh_pct_x100;
    uint16_t battery_mv;
} raw_sensors_t;

static uint8_t estimate_fill_pct(uint16_t distance_mm) {
    const uint16_t empty_mm = 420u;
    const uint16_t full_mm = 40u;
    if (distance_mm >= empty_mm) return 0u;
    if (distance_mm <= full_mm) return 100u;
    return (uint8_t)(((empty_mm - distance_mm) * 100u) / (empty_mm - full_mm));
}

int main(void) {
    raw_sensors_t sensors = {
        .distance_mm = 138u,
        .mass_grams = 9240u,
        .voc_index = 182u,
        .temp_c_x100 = 2315,
        .rh_pct_x100 = 5580u,
        .battery_mv = 3870u,
    };

    ws_bin_telemetry_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.stream = WS_STREAM_COMPOST;
    msg.fill_pct = estimate_fill_pct(sensors.distance_mm);
    msg.mass_grams = sensors.mass_grams;
    msg.voc_index = sensors.voc_index;
    msg.temp_c_x100 = sensors.temp_c_x100;
    msg.rh_pct_x100 = sensors.rh_pct_x100;
    msg.lid_open_count = 14u;
    msg.battery_mv = sensors.battery_mv;
    msg.deodorizer_active = (sensors.voc_index > 160u) ? 1u : 0u;

    printf("dock stream=%u fill=%u%% mass=%ug voc=%u deodorizer=%u\n",
           msg.stream, msg.fill_pct, msg.mass_grams, msg.voc_index, msg.deodorizer_active);
    return 0;
}
