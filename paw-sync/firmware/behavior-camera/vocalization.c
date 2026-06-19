/*
 * vocalization.c — PawSync vocalization classification from 6-mic array
 *
 * Classifies pet vocalizations (barks/whines/meows) into 6 categories:
 *   0=none, 1=pain, 2=anxiety, 3=alert, 4=play, 5=attention, 6=distress
 *
 * Pipeline:
 * 1. Voice Activity Detection (VAD) — detect when a vocalization occurs
 * 2. Compute Mel-spectrogram (64 mel bins × 128 frames)
 * 3. Run TFLite Micro CNN classifier
 * 4. Output class + confidence
 *
 * The 6-mic array also enables sound localization (which direction the
 * vocalization came from), useful for multi-pet homes.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "paw_protocol.h"

#define AUDIO_SAMPLE_RATE  16000
#define AUDIO_WINDOW_S     2
#define AUDIO_WINDOW_SIZE  (AUDIO_SAMPLE_RATE * AUDIO_WINDOW_S)
#define MEL_BINS           64
#define MEL_FRAMES         128
#define VAD_THRESHOLD      500   /* RMS threshold for voice activity */
#define VAD_MIN_DURATION   10    /* min samples above threshold */

/* ---- TFLite Micro model embedding (stub) ---- */
const unsigned char vocalization_model_data[] = {
    0x1c, 0x00, 0x00, 0x00, 0x54, 0x46, 0x4c, 0x33,
    0x00, 0x00, 0x00, 0x00,
};
const unsigned int vocalization_model_len = sizeof(vocalization_model_data);

/* ---- Mel filterbank (pre-computed for 16kHz, 64 bins, FFT=512) ---- */
#define FFT_SIZE 512
#define NUM_FILTERS 64

/* Mel scale conversion */
static float hz_to_mel(float hz) { return 2595.0f * log10f(1.0f + hz / 700.0f); }
static float mel_to_hz(float mel) { return 700.0f * (powf(10.0f, mel / 2595.0f) - 1.0f); }

/* ---- Simple FFT (radix-2 DIT) ---- */
static void fft(float *real, float *imag, int n)
{
    /* Bit-reversal permutation */
    int j = 0;
    for (int i = 1; i < n; i++) {
        int bit = n >> 1;
        while (j & bit) { j ^= bit; bit >>= 1; }
        j ^= bit;
        if (i < j) {
            float tr = real[i]; real[i] = real[j]; real[j] = tr;
            float ti = imag[i]; imag[i] = imag[j]; imag[j] = ti;
        }
    }
    /* Cooley-Tukey */
    for (int len = 2; len <= n; len <<= 1) {
        float ang = -2.0f * 3.14159265f / len;
        float wr = cosf(ang), wi = sinf(ang);
        for (int i = 0; i < n; i += len) {
            float wlr = 1.0f, wli = 0.0f;
            for (int k = 0; k < len / 2; k++) {
                float ur = real[i+k], ui = imag[i+k];
                float vr = real[i+k+len/2] * wlr - imag[i+k+len/2] * wli;
                float vi = real[i+k+len/2] * wli + imag[i+k+len/2] * wlr;
                real[i+k] = ur + vr;
                imag[i+k] = ui + vi;
                real[i+k+len/2] = ur - vr;
                imag[i+k+len/2] = ui - vi;
                float nwlr = wlr * wr - wli * wi;
                wli = wlr * wi + wli * wr;
                wlr = nwlr;
            }
        }
    }
}

/* ---- Compute Mel-spectrogram ---- */
static void compute_mel_spectrogram(const int16_t *audio, int n,
                                    float mel_out[MEL_FRAMES][MEL_BINS])
{
    int hop = (n - FFT_SIZE) / MEL_FRAMES;
    if (hop < 1) hop = 1;

    for (int frame = 0; frame < MEL_FRAMES; frame++) {
        float real[FFT_SIZE], imag[FFT_SIZE];
        int start = frame * hop;

        /* Apply Hanning window + zero-fill */
        for (int i = 0; i < FFT_SIZE; i++) {
            if (start + i < n) {
                float w = 0.5f * (1.0f - cosf(2.0f * 3.14159265f * i / (FFT_SIZE - 1)));
                real[i] = (float)audio[start + i] * w;
            } else {
                real[i] = 0;
            }
            imag[i] = 0;
        }

        fft(real, imag, FFT_SIZE);

        /* Power spectrum */
        float power[FFT_SIZE / 2];
        for (int i = 0; i < FFT_SIZE / 2; i++)
            power[i] = real[i] * real[i] + imag[i] * imag[i];

        /* Apply Mel filterbank (simplified triangular filters) */
        for (int m = 0; m < MEL_BINS; m++) {
            float mel_center = (float)(m + 1) / (MEL_BINS + 1);
            float hz_center = mel_to_hz(hz_to_mel(8000) * mel_center);
            int bin_center = (int)(hz_center * FFT_SIZE / AUDIO_SAMPLE_RATE);
            int bin_width = bin_center / 2 + 1;

            float sum = 0;
            for (int i = bin_center - bin_width; i <= bin_center + bin_width; i++) {
                if (i >= 0 && i < FFT_SIZE / 2) {
                    float weight = 1.0f - fabsf((float)(i - bin_center) / bin_width);
                    if (weight > 0) sum += power[i] * weight;
                }
            }
            mel_out[frame][m] = log10f(sum + 1.0f);
        }
    }
}

/* ---- Voice Activity Detection (VAD) ---- */
static int detect_vocalization(const int16_t *audio, int n)
{
    int above_threshold = 0;
    for (int i = 0; i < n; i += 100) {  /* check every 100 samples */
        float rms = 0;
        int end = (i + 100 < n) ? i + 100 : n;
        for (int j = i; j < end; j++)
            rms += (float)audio[j] * audio[j];
        rms = sqrtf(rms / (end - i));
        if (rms > VAD_THRESHOLD)
            above_threshold++;
    }
    return (above_threshold >= VAD_MIN_DURATION) ? 1 : 0;
}

/* ---- Heuristic vocalization classifier ---- */
/*
 * Uses spectral features to classify vocalizations when ML model unavailable:
 *   - High pitch + short duration → pain/distress
 *   - Sustained mid-pitch + rising → anxiety/whine
 *   - Sharp burst + high energy → alert bark
 *   - Variable pitch + short → play
 *   - Repetitive + mid-energy → attention
 */
static uint8_t heuristic_vocal_classify(const int16_t *audio, int n)
{
    /* Compute spectral centroid (brightness) */
    float centroid = 0, total_energy = 0;
    for (int i = 0; i < n; i += FFT_SIZE) {
        int chunk = (i + FFT_SIZE < n) ? FFT_SIZE : n - i;
        if (chunk < FFT_SIZE / 2) break;

        float real[FFT_SIZE], imag[FFT_SIZE];
        for (int j = 0; j < chunk && j < FFT_SIZE; j++) {
            real[j] = (float)audio[i + j];
            imag[j] = 0;
        }
        fft(real, imag, FFT_SIZE);

        float weighted_sum = 0, energy = 0;
        for (int j = 0; j < FFT_SIZE / 2; j++) {
            float mag = sqrtf(real[j] * real[j] + imag[j] * imag[j]);
            weighted_sum += (float)j * mag;
            energy += mag;
        }
        if (energy > 0) centroid += weighted_sum / energy;
        total_energy += energy;
    }

    /* Compute duration above threshold (vocalization length) */
    int duration_samples = 0;
    for (int i = 0; i < n; i += 100) {
        float rms = 0;
        int end = (i + 100 < n) ? i + 100 : n;
        for (int j = i; j < end; j++)
            rms += (float)audio[j] * audio[j];
        rms = sqrtf(rms / (end - i));
        if (rms > VAD_THRESHOLD) duration_samples += 100;
    }
    float duration_s = (float)duration_samples / AUDIO_SAMPLE_RATE;

    /* Classification rules */
    if (centroid > 8000 && duration_s < 0.5f)
        return 1;  /* pain — high pitch, short */
    if (centroid > 8000 && duration_s > 1.0f)
        return 6;  /* distress — high pitch, sustained */
    if (duration_s > 1.5f && centroid < 5000)
        return 2;  /* anxiety — sustained whine, mid-pitch */
    if (duration_s < 0.3f && total_energy > 1e8)
        return 3;  /* alert — sharp bark */
    if (centroid > 6000 && duration_s < 0.8f)
        return 4;  /* play — variable, mid-short */
    if (duration_s > 0.5f && centroid < 4000)
        return 5;  /* attention — repetitive, low-mid */

    return 3;  /* default: alert bark */
}

/* ---- Public API ---- */
void vocalization_classify(const int16_t *audio, int n,
                           uint8_t *class_out, uint8_t *conf_out)
{
    *class_out = 0;
    *conf_out = 0;

    /* VAD: only classify if vocalization detected */
    if (!detect_vocalization(audio, n))
        return;

    /* In production: compute Mel-spectrogram → TFLite Micro CNN.
     * For now: use heuristic classifier. */
    *class_out = heuristic_vocal_classify(audio, n);
    *conf_out = 70;
}