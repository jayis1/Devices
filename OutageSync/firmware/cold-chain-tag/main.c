#include <stdint.h>
#include <stdio.h>

static uint16_t estimate_hold_minutes(int16_t product_temp_c_x100, uint16_t door_open_seconds, uint8_t freezer_mode) {
    int base = freezer_mode ? 1800 : 240;
    int thermal_penalty = (product_temp_c_x100 > (freezer_mode ? -1200 : 500)) ? 180 : 40;
    int door_penalty = door_open_seconds / 6;
    int estimate = base - thermal_penalty - door_penalty;
    return (uint16_t)(estimate > 0 ? estimate : 0);
}

int main(void) {
    struct sample { int16_t t; uint16_t door; uint8_t freezer; } samples[] = {
        {-1825, 12, 1}, {-1410, 92, 1}, {380, 46, 0}, {690, 180, 0}
    };
    for (unsigned i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i) {
        uint16_t hold = estimate_hold_minutes(samples[i].t, samples[i].door, samples[i].freezer);
        printf("sample=%u temp=%.2fC door=%us freezer=%u hold=%umin\n",
               i,
               samples[i].t / 100.0,
               samples[i].door,
               samples[i].freezer,
               hold);
    }
    return 0;
}
