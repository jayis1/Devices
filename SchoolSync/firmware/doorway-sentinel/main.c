#include <stdint.h>
#include <stdio.h>
#include "../common/protocol.h"

int main(void) {
    ss_door_event_t ev = {
        .door_open = 1,
        .bag_present = 1,
        .lunch_present = 0,
        .missing_items = 1,
        .uwb_range_cm = 95,
        .eta_minutes = 8,
        .readiness_score = 71,
        .reserved = 0,
    };
    printf("door open=%u bag=%u lunch=%u missing=%u eta=%u readiness=%u\n",
           ev.door_open,
           ev.bag_present,
           ev.lunch_present,
           ev.missing_items,
           ev.eta_minutes,
           ev.readiness_score);
    return 0;
}
