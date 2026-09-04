#include <stdint.h>
#include <stdio.h>
#include "../common/protocol.h"

static wsync_water_quality_t sample_water_quality(uint16_t seq) {
    wsync_water_quality_t out;
    out.ph_x100 = (int16_t)(652 - (seq % 5));
    out.ec_us_cm = (uint16_t)(510 + seq * 4u);
    out.orp_mv = (int16_t)(215 - (int16_t)(seq * 2u));
    out.turbidity_ntu_x10 = (uint16_t)(18 + seq * 3u);
    out.pressure_kpa = (uint16_t)(402 + seq * 2u);
    out.temp_c_x100 = (int16_t)(1090 + (int16_t)seq * 3);
    out.advisory_state = (out.turbidity_ntu_x10 > 35u) ? WSYNC_ALERT_TREAT : WSYNC_ALERT_SAFE;
    out.reserved = 0u;
    return out;
}

int main(void) {
    for (uint16_t seq = 1; seq <= 4; ++seq) {
        wsync_water_quality_t wq = sample_water_quality(seq);
        printf("wq seq=%u ph=%.2f ec=%u orp=%d turbidity=%.1f pressure=%u advisory=%u\n",
               seq,
               wq.ph_x100 / 100.0,
               wq.ec_us_cm,
               wq.orp_mv,
               wq.turbidity_ntu_x10 / 10.0,
               wq.pressure_kpa,
               wq.advisory_state);
    }
    return 0;
}
