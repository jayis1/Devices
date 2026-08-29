#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static bool inhibit_start(uint16_t co2_ppm, uint8_t co_alarm, uint16_t enclosure_temp_c_x100, uint16_t fuel_pct) {
    if (co_alarm) return true;
    if (co2_ppm > 2500) return true;
    if (enclosure_temp_c_x100 > 5500) return true;
    if (fuel_pct < 12) return true;
    return false;
}

int main(void) {
    struct sample { uint16_t co2; uint8_t co; uint16_t temp; uint16_t fuel; } samples[] = {
        {810, 0, 2800, 88}, {1450, 0, 3600, 24}, {2900, 0, 4100, 60}, {600, 1, 2500, 70}
    };
    for (unsigned i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i) {
        printf("sample=%u safe_start=%u\n", i, inhibit_start(samples[i].co2, samples[i].co, samples[i].temp, samples[i].fuel) ? 0u : 1u);
    }
    return 0;
}
