/*
 * PostureSync EMG Processing Module
 * 8-channel surface EMG feature extraction
 */

#include "emg_processing.h"
#include <math.h>
#include <string.h>

void emg_compute_rms(const int32_t *samples, int count, uint16_t *rms_out, float mvc)
{
    float sum_sq = 0;
    for (int i = 0; i < count; i++) {
        float v = (float)samples[i] / 1000.0f; /* uV */
        sum_sq += v * v;
    }
    float rms = sqrtf(sum_sq / count);
    float normalized = rms / mvc;
    if (normalized > 65.535f) normalized = 65.535f;
    *rms_out = (uint16_t)(normalized * 1000.0f);
}

uint8_t emg_compute_asymmetry(const uint16_t *rms)
{
    /* Pairs: (0,1)=Trap, (2,3)=Erector, (4,5)=SCM, (6,7)=Rectus */
    float max_asym = 0;
    for (int pair = 0; pair < 4; pair++) {
        float left = rms[pair * 2];
        float right = rms[pair * 2 + 1];
        if (left + right > 0) {
            float asym = fabsf(left - right) / (left + right) * 100.0f;
            if (asym > max_asym) max_asym = asym;
        }
    }
    return (uint8_t)max_asym;
}

float emg_compute_median_freq(const int32_t *samples, int count)
{
    /* Simplified median frequency via FFT
     * In production: use CMSIS-DSP arm_rfft_q31 */
    /* Placeholder: compute zero-crossing rate as proxy */
    int crossings = 0;
    for (int i = 1; i < count; i++) {
        if ((samples[i] > 0 && samples[i-1] < 0) ||
            (samples[i] < 0 && samples[i-1] > 0)) {
            crossings++;
        }
    }
    /* Approximate median frequency from crossings */
    return (float)crossings / 2.0f;
}

uint8_t emg_compute_fatigue(const uint16_t *current_rms, const float *initial_mvc)
{
    float decline_sum = 0;
    for (int i = 0; i < 8; i++) {
        if (initial_mvc[i] > 0) {
            float ratio = (float)current_rms[i] / (initial_mvc[i] * 1000.0f);
            if (ratio > 1.0f) ratio = 1.0f;
            decline_sum += (1.0f - ratio);
        }
    }
    uint8_t fatigue = (uint8_t)(decline_sum / 8.0f * 100.0f);
    if (fatigue > 100) fatigue = 100;
    return fatigue;
}

float emg_compute_cocontraction(const uint16_t *rms)
{
    /* Co-contraction ratio: antagonist/agonist
     * For spine: erector spinae (extensor) vs rectus abdominis (flexor) */
    float extensor = (rms[2] + rms[3]) / 2.0f;  /* L+R Erector */
    float flexor = (rms[6] + rms[7]) / 2.0f;    /* L+R Rectus */

    if (extensor < 1.0f) extensor = 1.0f;
    if (flexor < 1.0f) flexor = 1.0f;

    float ratio = flexor / extensor;
    if (ratio > 1.0f) ratio = 1.0f / ratio;
    return ratio;
}