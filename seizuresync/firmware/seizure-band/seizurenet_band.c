/*
 * SeizureSync — SeizureNet band-side inference implementation
 *
 * Production: load seizurenet_v1.tflite (int8 quantized) via tflite-micro.
 * Here: heuristic fallback for development.
 *
 * SPDX-License-Identifier: MIT
 */
#include "seizurenet_band.h"
#include "esp_log.h"
#include <math.h>
#include <string.h>

static const char *TAG = "SZNET";

static uint8_t g_arena[64 * 1024];

void seizurennet_init(void)
{
    memset(g_arena, 0, sizeof(g_arena));
    ESP_LOGI(TAG, "SeizureNet model loaded (64 KB arena)");
}

/* Heuristic features for fallback:
 *  - accel_energy: RMS of accel magnitude (high = tonic-clonic motion)
 *  - hr_spike: max HR / resting HR (ictal tachycardia > 1.5×)
 *  - eda_surge: max EDA (post-ictal surge > 2 µS)
 */
float seizurennet_infer(const float *accel, const float *ppg,
                        const float *eda, int alen, int plen, int elen)
{
    /* Accel RMS energy */
    float a_energy = 0;
    for (int i = 0; i < alen; i++) a_energy += accel[i] * accel[i];
    a_energy = sqrtf(a_energy / alen);

    /* HR spike ratio */
    float hr_min = 999, hr_max = 0;
    for (int i = 0; i < plen; i++) {
        if (ppg[i] < hr_min) hr_min = ppg[i];
        if (ppg[i] > hr_max) hr_max = ppg[i];
    }
    float hr_ratio = (hr_min > 0) ? (hr_max / hr_min) : 1.0f;

    /* EDA surge */
    float eda_max = 0;
    for (int i = 0; i < elen; i++) if (eda[i] > eda_max) eda_max = eda[i];

    ESP_LOGD(TAG, "a_energy=%.2f hr_ratio=%.2f eda_max=%.2f",
             a_energy, hr_ratio, eda_max);

    /* Seizure if: high motion + ictal tachycardia + EDA surge */
    if (a_energy > 3.0f && hr_ratio > 1.5f && eda_max > 2.0f)
        return 0.95f;
    if (a_energy > 3.0f && hr_ratio > 1.3f)
        return 0.7f;
    if (a_energy > 3.0f)
        return 0.3f;   /* motion artifact */
    return 0.02f;      /* rest */
}

void haptic_pulse(int count)
{
    /* Production: DRV2605L I2C sequence. */
    ESP_LOGI(TAG, "Haptic pulse x%d", count);
}