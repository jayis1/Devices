/*
 * prosody.c — CalmGrid voice prosody stress classification
 *
 * Classifies voice stress from acoustic prosody features — **without
 * transcribing or storing any speech content**. Only acoustic features
 * are extracted on-device; no audio is stored or transmitted.
 *
 * Prosody features extracted:
 *   - F0 (fundamental frequency / pitch) mean + variability
 *   - Jitter (pitch period irregularity)
 *   - Shimmer (amplitude irregularity)
 *   - Speech rate (syllabic rate from energy envelope)
 *   - Energy mean + variability
 *   - Spectral tilt (high/low frequency energy ratio)
 *   - HNR (harmonics-to-noise ratio)
 *
 * Stress signatures in prosody:
 *   - Raised F0 (higher pitch under stress)
 *   - Increased F0 variability (emotional activation)
 *   - Increased jitter + shimmer (vocal tension)
 *   - Faster speech rate (urgency)
 *   - Higher energy (effortful speech)
 *   - Flatter spectral tilt (brighter, more tense voice)
 *
 * Classifier: 1D-CNN on feature vector → 4 classes:
 *   0=calm, 1=neutral, 2=elevated, 3=high-stress
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "calm_protocol.h"

#define AUDIO_SAMPLE_RATE  16000
#define FFT_SIZE           512
#define NUM_PROSODY_FEATS  9
#define PROSODY_FRAMES     8

/* ---- TFLite Micro model embedding (stub) ---- */
const unsigned char prosody_model_data[] = {
    0x1c, 0x00, 0x00, 0x00, 0x54, 0x46, 0x4c, 0x33,
    0x00, 0x00, 0x00, 0x00,
};
const unsigned int prosody_model_len = sizeof(prosody_model_data);

/* ---- Simple FFT (radix-2 DIT) ---- */
static void fft(float *re, float *im, int n)
{
    /* Bit-reversal permutation */
    int j = 0;
    for (int i = 1; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j) {
            float tr = re[i]; re[i] = re[j]; re[j] = tr;
            float ti = im[i]; im[i] = im[j]; im[j] = ti;
        }
    }
    /* Cooley-Tukey */
    for (int len = 2; len <= n; len <<= 1) {
        float ang = -2.0f * 3.14159265f / len;
        float wr = cosf(ang), wi = sinf(ang);
        for (int i = 0; i < n; i += len) {
            float cr = 1.0f, ci = 0.0f;
            for (int k = 0; k < len / 2; k++) {
                float ur = re[i + k], ui = im[i + k];
                float vr = re[i + k + len/2] * cr - im[i + k + len/2] * ci;
                float vi = re[i + k + len/2] * ci + im[i + k + len/2] * cr;
                re[i + k] = ur + vr;
                im[i + k] = ui + vi;
                re[i + k + len/2] = ur - vr;
                im[i + k + len/2] = ui - vi;
                float ncr = cr * wr - ci * wi;
                ci = cr * wi + ci * wr;
                cr = ncr;
            }
        }
    }
}

/* ---- Autocorrelation-based F0 estimation ---- */
static float estimate_f0(const int16_t *audio, int n)
{
    /* Normalize */
    float sig[512];
    int len = n < 512 ? n : 512;
    float mean = 0;
    for (int i = 0; i < len; i++) mean += audio[i];
    mean /= len;
    for (int i = 0; i < len; i++) sig[i] = audio[i] - mean;

    /* Autocorrelation for lag 40-400 (40Hz-400Hz at 16kHz) */
    float max_acf = 0;
    int best_lag = 0;
    for (int lag = 40; lag <= 400 && lag < len; lag++) {
        float acf = 0;
        for (int i = 0; i < len - lag; i++)
            acf += sig[i] * sig[i + lag];
        acf /= (len - lag);
        if (acf > max_acf) {
            max_acf = acf;
            best_lag = lag;
        }
    }
    if (best_lag == 0) return 0;
    return (float)AUDIO_SAMPLE_RATE / best_lag;
}

/* ---- Energy (RMS) ---- */
static float compute_energy(const int16_t *audio, int n)
{
    float sum_sq = 0;
    for (int i = 0; i < n; i++)
        sum_sq += (float)audio[i] * audio[i];
    return sqrtf(sum_sq / n);
}

/* ---- Spectral tilt (ratio of high to low frequency energy) ---- */
static float compute_spectral_tilt(const int16_t *audio, int n)
{
    float re[FFT_SIZE], im[FFT_SIZE];
    int len = n < FFT_SIZE ? n : FFT_SIZE;
    for (int i = 0; i < FFT_SIZE; i++) {
        re[i] = (i < len) ? (float)audio[i] : 0.0f;
        im[i] = 0.0f;
    }
    fft(re, im, FFT_SIZE);

    float low_energy = 0, high_energy = 0;
    int low_end = FFT_SIZE / 8;   /* 0-1kHz */
    int high_start = FFT_SIZE / 4; /* 2-8kHz */
    for (int i = 0; i < low_end; i++)
        low_energy += re[i]*re[i] + im[i]*im[i];
    for (int i = high_start; i < FFT_SIZE / 2; i++)
        high_energy += re[i]*re[i] + im[i]*im[i];

    if (low_energy < 1.0f) return 0;
    return high_energy / low_energy;
}

/* ---- Speech rate from energy envelope zero-crossings ---- */
static float estimate_speech_rate(const int16_t *audio, int n)
{
    /* Compute energy envelope in 20ms windows */
    int win = 320;  /* 20ms @ 16kHz */
    int n_windows = n / win;
    if (n_windows < 2) return 0;

    float env[256];
    for (int w = 0; w < n_windows && w < 256; w++) {
        env[w] = compute_energy(audio + w * win, win);
    }

    /* Count energy peaks (syllable approximation) */
    float mean_env = 0;
    for (int w = 0; w < n_windows; w++) mean_env += env[w];
    mean_env /= n_windows;

    int peaks = 0;
    for (int w = 1; w < n_windows - 1; w++) {
        if (env[w] > mean_env * 1.5f && env[w] > env[w-1] && env[w] > env[w+1])
            peaks++;
    }
    /* Syllables per second */
    return (float)peaks / (n_windows * 0.02f);
}

/* ---- Extract prosody feature vector ---- */
void extract_prosody_features(const int16_t *audio, int n,
                               float *features, int *n_features)
{
    *n_features = NUM_PROSODY_FEATS;

    float f0 = estimate_f0(audio, n);
    float energy = compute_energy(audio, n);
    float tilt = compute_spectral_tilt(audio, n);
    float rate = estimate_speech_rate(audio, n);

    /* F0 variability (std across sub-windows) */
    float f0_vals[4];
    int sub_len = n / 4;
    for (int i = 0; i < 4; i++)
        f0_vals[i] = estimate_f0(audio + i * sub_len, sub_len);
    float f0_mean = 0, f0_var = 0;
    for (int i = 0; i < 4; i++) f0_mean += f0_vals[i];
    f0_mean /= 4;
    for (int i = 0; i < 4; i++) f0_var += (f0_vals[i] - f0_mean) * (f0_vals[i] - f0_mean);
    f0_var = sqrtf(f0_var / 4);

    /* Jitter (simplified: F0 period irregularity) */
    float jitter = f0_mean > 0 ? f0_var / f0_mean : 0;

    /* Energy variability */
    float e_vals[4];
    for (int i = 0; i < 4; i++)
        e_vals[i] = compute_energy(audio + i * sub_len, sub_len);
    float e_var = 0;
    for (int i = 0; i < 4; i++) e_var += (e_vals[i] - energy) * (e_vals[i] - energy);
    e_var = sqrtf(e_var / 4);
    float shimmer = energy > 0 ? e_var / energy : 0;

    /* HNR (simplified: ratio of periodic to total energy) */
    float hnr = f0 > 0 ? (f0_var < 1.0f ? 20.0f : 10.0f / f0_var) : 0;

    features[0] = f0;
    features[1] = f0_var;
    features[2] = jitter;
    features[3] = shimmer;
    features[4] = rate;
    features[5] = energy / 32768.0f;  /* normalized */
    features[6] = e_var / 32768.0f;
    features[7] = tilt;
    features[8] = hnr;
}

/* ---- Heuristic prosody stress classifier ---- */
/*
 * When the TFLite model is not available, uses prosody feature thresholds:
 *   - High F0 + high variability → high-stress
 *   - Moderate F0 → elevated
 *   - Normal F0 + low jitter → calm/neutral
 *
 * F0 elevation is relative to a personal baseline (set during calibration).
 * For development, uses absolute thresholds.
 */
static uint8_t heuristic_prosody(const float *features, int n,
                                  uint8_t *confidence, int16_t *f0_dev)
{
    float f0 = features[0];
    float f0_var = features[1];
    float jitter = features[2];
    float rate = features[4];

    *f0_dev = 0;

    /* F0 deviation from typical baseline (150 Hz for males, 220 for females)
     * Using 180 Hz as a general baseline */
    float baseline_f0 = 180.0f;
    float f0_ratio = f0 / baseline_f0;
    *f0_dev = (int16_t)((f0_ratio - 1.0f) * 1200.0f);  /* cents * 10 */

    /* Stress score from prosody features */
    float stress = 0;
    if (f0_ratio > 1.15f) stress += 30;  /* pitch raised >15% */
    if (f0_var > 30.0f) stress += 20;    /* high pitch variability */
    if (jitter > 0.05f) stress += 15;    /* vocal tension */
    if (rate > 5.0f) stress += 20;       /* fast speech */
    if (features[7] > 0.5f) stress += 15; /* bright spectral tilt */

    if (stress >= 60) {
        *confidence = 75;
        return 3;  /* high-stress */
    } else if (stress >= 35) {
        *confidence = 70;
        return 2;  /* elevated */
    } else if (stress >= 15) {
        *confidence = 65;
        return 1;  /* neutral */
    } else {
        *confidence = 80;
        return 0;  /* calm */
    }
}

/* ---- Main classification entry point ---- */
uint8_t classify_prosody(const int16_t *audio, int n,
                          uint8_t *confidence, int16_t *f0_dev)
{
    float features[NUM_PROSODY_FEATS];
    int n_feat;
    extract_prosody_features(audio, n, features, &n_feat);

    /* In production: run TFLite Micro inference with prosody_model_data
     * For now: use heuristic classifier */
    return heuristic_prosody(features, n_feat, confidence, f0_dev);
}