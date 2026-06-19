/*
 * pressure_matrix.c — SoleGuard insole pressure-matrix feature extraction
 *
 * Helper functions for scanning the 24-FSR array and computing per-step
 * peak pressure, pressure-time integral (PTI), contact area, and
 * center-of-pressure (CoP) trajectory.
 *
 * SPDX-License-Identifier: MIT
 */
#include "sole_protocol.h"
#include <string.h>

#define PRESSURE_KPA_MAX   500
#define CONTACT_THRESH_Q   40   /* ~78 kPa contact threshold (out of 255) */

/* Convert 8-bit pressure to kPa (fixed-point centi-kPa) */
uint16_t pressure_to_kpa_centic(uint8_t q)
{
    return (uint16_t)((uint32_t)q * (PRESSURE_KPA_MAX * 100u) / 255u);
}

/* Contact area: count of sensors above threshold (out of 24) */
uint8_t pressure_contact_area(const uint8_t pressure[24])
{
    uint8_t n = 0;
    for (int i = 0; i < 24; i++)
        if (pressure[i] > CONTACT_THRESH_Q) n++;
    return n;
}

/* Center-of-pressure in normalized [0,1] x [0,1] shoe coordinates.
   Sensor layout grid (approx): 6 rows x 4 cols, row 0 = heel. */
void pressure_cop(const uint8_t pressure[24], float *cop_x, float *cop_y)
{
    uint32_t total = 0, sx = 0, sy = 0;
    for (int i = 0; i < 24; i++) {
        uint8_t row = (uint8_t)(i / 4);
        uint8_t col = (uint8_t)(i % 4);
        uint32_t w  = pressure[i];
        sx += w * col;
        sy += w * row;
        total += w;
    }
    if (total == 0) { *cop_x = 0.5f; *cop_y = 0.5f; return; }
    *cop_x = (float)sx / (float)(total * 3);  /* 0..1 */
    *cop_y = (float)sy / (float)(total * 5);  /* 0..1 */
}

/* Per-zone peak pressure (0-255 scale) — wraps sole_zone_peak_pressure */
uint8_t zone_peak(const uint8_t pressure[24], sole_zone_t zone)
{
    return sole_zone_peak_pressure(pressure, zone);
}

/* Per-zone PTI (pressure-time integral) in centi-kPa*s, given the number
   of 10-ms samples in the stance window. */
uint32_t zone_pti_centic(const uint8_t pressure[24], sole_zone_t zone, uint8_t samples)
{
    uint32_t sum = sole_zone_pti_sum(pressure, zone, samples);
    /* sum = (q * samples) summed over 4 sensors; q in 0..255 -> kPa = q*500/255
       PTI = mean_kPa * time_s = (sum/4)*(500/255)*(samples*0.01) *100 centi */
    uint64_t kpa_s = (uint64_t)sum * 500u * (uint64_t)samples * 1u;
    kpa_s /= (4u * 255u * 100u);
    return (uint32_t)(kpa_s); /* centi-kPa*s */
}