#include <stdint.h>
#include <stdio.h>
#include "../common/protocol.h"

int main(void) {
    ss_transit_event_t ev = {
        .boarded = 1,
        .route_ok = 1,
        .child_present = 1,
        .backpack_present = 1,
        .eta_minutes = 14,
        .speed_dmps = 125,
        .battery_mv = 4010,
        .alert_code = 0,
    };
    printf("transit boarded=%u route_ok=%u eta=%u speed_dmps=%u battery=%u\n",
           ev.boarded,
           ev.route_ok,
           ev.eta_minutes,
           ev.speed_dmps,
           ev.battery_mv);
    return 0;
}
