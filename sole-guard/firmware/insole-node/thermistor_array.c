/*
 * thermistor_array.c — SoleGuard insole NTC thermistor array driver
 *
 * 8x NTC 10k @25C (B=3977) in voltage-divider with 10k pull-up,
 * scanned via the second analog mux into nRF52840 ADC channel 1.
 *
 * SPDX-License-Identifier: MIT
 */
#include <zephyr/drivers/adc.h>
#include "sole_protocol.h"
#include <math.h>

#define B_PARAM    3977.0f
#define R25        10000.0f
#define T0         298.15f

extern const struct device *adc_dev;
extern struct adc_channel_cfg fsr_chan_cfg; /* reuse pattern; channel 1 for therm */
extern void select_mux_channel(uint8_t ch);

static int16_t ntc_to_centic(uint16_t adc_val, uint16_t adc_max)
{
    if (adc_val == 0 || adc_val >= adc_max) return 0;
    float r = (float)adc_val / (float)(adc_max - adc_val);
    float R = R25 * r;
    float invT = 1.0f / T0 + (1.0f / B_PARAM) * logf(R / R25);
    float T = (1.0f / invT) - 273.15f;
    return (int16_t)(T * 100.0f);
}

/* Scan all 8 thermistors into temp_centic[] (centi-degC). */
void thermistor_scan(int16_t temp_centic[8], uint16_t adc_max)
{
    struct adc_sequence seq = {
        .channels    = BIT(1),
        .buffer      = NULL,
        .buffer_size = sizeof(uint16_t),
        .resolution  = 12,
        .oversampling = 8,
    };
    /* thermistors live on mux channels 8..15 */
    for (int i = 0; i < 8; i++) {
        select_mux_channel((uint8_t)(8 + i));
        k_busy_wait(250);
        uint16_t buf = 0;
        seq.buffer = &buf;
        if (adc_read(adc_dev, &seq) == 0)
            temp_centic[i] = ntc_to_centic(buf, adc_max);
    }
}

/* Compute bilateral asymmetry: |L[i] - R[i]| for each of 8 zones.
 * Returns the maximum asymmetry in centi-degC and the zone index. */
int16_t thermistor_asymmetry_max(const int16_t left[8], const int16_t right[8],
                                 uint8_t *worst_zone)
{
    int16_t max_diff = 0;
    *worst_zone = 0;
    for (int i = 0; i < 8; i++) {
        int16_t d = left[i] - right[i];
        if (d < 0) d = (int16_t)-d;
        if (d > max_diff) {
            max_diff = d;
            *worst_zone = (uint8_t)i;
        }
    }
    return max_diff;
}