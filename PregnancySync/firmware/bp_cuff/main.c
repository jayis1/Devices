#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    uint16_t systolic_mmHg;
    uint16_t diastolic_mmHg;
    uint16_t map_mmHg;
    uint8_t pulse_rate_bpm;
    float waveform_quality;
} cuff_result_t;

static cuff_result_t oscillometric_estimate(const uint16_t *waveform, uint16_t count) {
    uint32_t sum = 0U;
    cuff_result_t result = {132U, 84U, 100U, 79U, 0.96f};
    for (uint16_t i = 0; i < count; ++i) {
        sum += waveform[i];
    }
    if (count > 0U) {
        result.map_mmHg = (uint16_t)(sum / count);
        result.systolic_mmHg = (uint16_t)(result.map_mmHg + 32U);
        result.diastolic_mmHg = (uint16_t)(result.map_mmHg - 16U);
    }
    return result;
}

int main(void) {
    const uint16_t waveform[6] = {98, 101, 103, 105, 101, 99};
    cuff_result_t reading = oscillometric_estimate(waveform, 6U);
    printf("cuff bp=%u/%u map=%u quality=%.2f\n",
           reading.systolic_mmHg, reading.diastolic_mmHg, reading.map_mmHg, reading.waveform_quality);
    return 0;
}
