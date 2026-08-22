#include <stdint.h>
#include <stdio.h>
#include "../common/protocol.h"

static us_env_event_t sample_environment(void) {
    us_env_event_t event = {0};
    event.temp_c_x100 = 2460;
    event.rh_pct_x100 = 6840;
    event.voc_index = 121u;
    event.leak_detected = 0u;
    event.fan_active = 1u;
    event.lux = 14u;
    return event;
}

int main(void) {
    us_env_event_t event = sample_environment();
    printf("bath-sentinel temp=%d rh=%u voc=%u leak=%u fan=%u\n",
           event.temp_c_x100,
           event.rh_pct_x100,
           event.voc_index,
           event.leak_detected,
           event.fan_active);
    return 0;
}
