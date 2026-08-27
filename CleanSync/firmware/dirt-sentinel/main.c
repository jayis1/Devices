#include <stdint.h>
#include <stdio.h>
#include "../common/protocol.h"

static uint8_t clamp_u8(int value) {
    if (value < 0) return 0u;
    if (value > 255) return 255u;
    return (uint8_t)value;
}

int main(void) {
    cs_dirt_telemetry_t sample = {
        .dust_index = 58,
        .traffic_score = 81,
        .wet_floor_pct = 36,
        .odor_index = 9,
        .temp_c_x10 = 243,
        .humidity_x10 = 612,
        .battery_mv = 3015,
    };

    int hygiene_pressure = sample.dust_index + sample.traffic_score / 2 + sample.wet_floor_pct;
    uint8_t alert = clamp_u8(hygiene_pressure / 3);

    printf("dirt dust=%u traffic=%u wet=%u humidity=%.1f battery=%u alert=%u\n",
           sample.dust_index,
           sample.traffic_score,
           sample.wet_floor_pct,
           sample.humidity_x10 / 10.0,
           sample.battery_mv,
           alert);
    return 0;
}
