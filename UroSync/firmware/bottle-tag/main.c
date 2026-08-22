#include <stdint.h>
#include <stdio.h>
#include "../common/protocol.h"

static us_bottle_event_t compute_day_metrics(uint16_t mass_g_before, uint16_t mass_g_after, uint16_t day_total_ml) {
    us_bottle_event_t event = {0};
    event.bottle_mass_g = mass_g_after;
    event.consumed_ml_day = day_total_ml + (uint16_t)(mass_g_before - mass_g_after);
    event.sip_count_day = 11u;
    event.adherence_score = (uint8_t)(event.consumed_ml_day > 1500u ? 88u : 57u);
    event.reminder_acked = 1u;
    return event;
}

int main(void) {
    us_bottle_event_t event = compute_day_metrics(780u, 620u, 1240u);
    printf("bottle remaining_g=%u consumed_ml=%u adherence=%u\n",
           event.bottle_mass_g,
           event.consumed_ml_day,
           event.adherence_score);
    return 0;
}
