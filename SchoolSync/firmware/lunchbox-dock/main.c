#include <stdint.h>
#include <stdio.h>
#include "../common/protocol.h"

int main(void) {
    ss_lunch_status_t lunch = {
        .packed = 1,
        .ice_pack_present = 1,
        .taken = 0,
        .risk_level = 1,
        .mass_grams = 742,
        .plate_temp_c_x100 = 530,
        .ambient_temp_c_x100 = 2210,
        .safe_minutes_remaining = 356,
    };
    printf("lunch packed=%u ice=%u mass=%u safe_min=%u risk=%u\n",
           lunch.packed,
           lunch.ice_pack_present,
           lunch.mass_grams,
           lunch.safe_minutes_remaining,
           lunch.risk_level);
    return 0;
}
