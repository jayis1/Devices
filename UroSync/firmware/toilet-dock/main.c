#include <stdint.h>
#include <stdio.h>
#include "../common/protocol.h"

static us_void_event_t analyze_sample(uint16_t mass_delta_g,
                                      uint16_t conductivity_proxy,
                                      uint8_t strip_leukocyte,
                                      uint8_t strip_nitrite) {
    us_void_event_t event = {0};
    event.event_id = 0x9001u;
    event.volume_ml = mass_delta_g;
    event.peak_flow_ml_min = 155u;
    event.flow_duration_s = 19u;
    event.color_index = 6u;
    event.leukocyte = strip_leukocyte;
    event.nitrite = strip_nitrite;
    event.blood = 0u;
    event.protein = 1u;
    event.ketone = 0u;
    event.glucose = 0u;
    event.ph_bin = 6u;
    event.sg_q1000 = conductivity_proxy;
    return event;
}

int main(void) {
    us_void_event_t event = analyze_sample(385u, 1022u, 1u, 0u);
    printf("toilet-dock event=%u volume=%u sg=%u leuk=%u nit=%u\n",
           event.event_id,
           event.volume_ml,
           event.sg_q1000,
           event.leukocyte,
           event.nitrite);
    return 0;
}
