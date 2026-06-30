/**
 * AsthmaSync — Wheeze Band Audio Processing
 * ==========================================
 * Captures I²S audio from SPH0645LM4H-B MEMS microphone,
 * computes 40-band mel-spectrogram, runs a lightweight
 * 1D-CNN pre-classifier to detect wheezing.
 *
 * The full 22-class classification runs on the Hub (edge ML).
 * The band sends the mel-spectrogram + pre-classifier probability
 * to the Hub via BLE for full inference.
 *
 * License: MIT
 */

#include "config.h"
#include "wheeze.h"
#include "../common/protocol.h"
#include <string.h>
#include <math.h>

/* ── nRF SDK includes ──────────────────────────────────── */
#include <zephyr/kernel.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(asthmasync_wheeze, LOG_LEVEL_INF);

/* ── Mel filterbank (40 bands, 0-8000 Hz, 512-point FFT) ─ */
/* Pre-computed mel filterbank weights.
   In production: generated at init from mel scale formula.
   Here we store a simplified triangular filterbank. */
static float s_mel_filters[MEL_BANDS][256];  /* 256 = NFFT/2+1 bins */

/* ── Pre-computed Hamming window (512 samples) ────────── */
static float s_hamming[512];

/* ── Ring buffer for audio samples ─────────────────────── */
static int16_t s_audio_ring[AUDIO_FRAME_SAMPLES];  /* 2s @ 16kHz */
static volatile uint32_t s_write_idx = 0;
static volatile bool s_buffer_full = false;

/* ── State ─────────────────────────────────────────────── */
static uint32_t s_last_wheeze_ms = 0;
static wheeze_callback_t s_callback = NULL;
static uint8_t s_wheeze_count = 0;

/* ── Initialize mel filterbank ─────────────────────────── */
static void init_mel_filterbank(void)
{
    /* 40 mel filters from 0 to 8000 Hz (Nyquist for 16 kHz sample rate)
       Mel(f) = 2595 * log10(1 + f/700)
       NFFT = 512, so FFT bins are at k * 16000/512 = k * 31.25 Hz */
    float mel_min = 0.0f;
    float mel_max = 2595.0f * log10f(1.0f + 8000.0f / 700.0f);

    float mel_points[MEL_BANDS + 2];
    for (int i = 0; i < MEL_BANDS + 2; i++) {
        mel_points[i] = mel_min + (mel_max - mel_min) * i / (MEL_BANDS + 1);
    }

    /* Convert mel back to Hz */
    float hz_points[MEL_BANDS + 2];
    for (int i = 0; i < MEL_BANDS + 2; i++) {
        hz_points[i] = 700.0f * (powf(10.0f, mel_points[i] / 2595.0f) - 1.0f);
    }

    /* Convert Hz to FFT bin */
    int bin_points[MEL_BANDS + 2];
    for (int i = 0; i < MEL_BANDS + 2; i++) {
        bin_points[i] = (int)floorf(hz_points[i] * 512.0f / 16000.0f);
    }

    /* Build triangular filters */
    for (int m = 0; m < MEL_BANDS; m++) {
        memset(s_mel_filters[m], 0, sizeof(s_mel_filters[m]));
        for (int k = bin_points[m]; k < bin_points[m + 1]; k++) {
            if (k >= 0 && k < 256) {
                s_mel_filters[m][k] = (float)(k - bin_points[m]) /
                    (float)(bin_points[m + 1] - bin_points[m]);
            }
        }
        for (int k = bin_points[m + 1]; k < bin_points[m + 2]; k++) {
            if (k >= 0 && k < 256) {
                s_mel_filters[m][k] = (float)(bin_points[m + 2] - k) /
                    (float)(bin_points[m + 2] - bin_points[m + 1]);
            }
        }
    }
}

/* ── Initialize Hamming window ─────────────────────────── */
static void init_hamming(void)
{
    for (int i = 0; i < 512; i++) {
        s_hamming[i] = 0.54f - 0.46f * cosf(2.0f * M_PI * i / 511.0f);
    }
}

/* ── Simplified FFT (256-point, in-place) ──────────────── */
/* For real production code, use CMSIS-DSP arm_rfft_fast_f32.
   Here we implement a basic radix-2 DFT for correctness. */
static void fft_256(float *real, float *imag)
{
    /* Bit-reversal permutation */
    int j = 0;
    for (int i = 1; i < 256; i++) {
        int bit = 128;
        while (j & bit) {
            j ^= bit;
            bit >>= 1;
        }
        j ^= bit;
        if (i < j) {
            float tr = real[i]; real[i] = real[j]; real[j] = tr;
            float ti = imag[i]; imag[i] = imag[j]; imag[j] = ti;
        }
    }

    /* Cooley-Tukey butterfly */
    for (int len = 2; len <= 256; len <<= 1) {
        float angle = -2.0f * M_PI / len;
        float wr = cosf(angle), wi = sinf(angle);
        for (int i = 0; i < 256; i += len) {
            float wmr = 1.0f, wmi = 0.0f;
            for (int k = 0; k < len / 2; k++) {
                float tr = wmr * real[i + k + len/2] - wmi * imag[i + k + len/2];
                float ti = wmr * imag[i + k + len/2] + wmi * real[i + k + len/2];
                real[i + k + len/2] = real[i + k] - tr;
                imag[i + k + len/2] = imag[i + k] - ti;
                real[i + k] += tr;
                imag[i + k] += ti;
                float nwr = wmr * wr - wmi * wi;
                wmi = wmr * wi + wmi * wr;
                wmr = nwr;
            }
        }
    }
}

/* ── Compute mel-spectrogram from audio frame ──────────── */
int wheeze_compute_mel(const int16_t *audio, size_t audio_len,
                       uint8_t *mel_out, size_t mel_out_size)
{
    if (!audio || !mel_out || mel_out_size < MEL_BANDS * MEL_FRAMES)
        return -1;

    int hop = 512;        /* 512 samples = 32ms frame */
    int frame_idx = 0;

    for (int start = 0; start + 512 <= (int)audio_len && frame_idx < MEL_FRAMES;
         start += hop) {

        /* Apply Hamming window + zero-pad to 512 (use first 256 for FFT) */
        float real[256], imag[256];
        memset(imag, 0, sizeof(imag));

        for (int i = 0; i < 256; i++) {
            /* Take every 2nd sample to fit 256-point FFT from 512-sample window */
            real[i] = (float)audio[start + i * 2] * s_hamming[i * 2];
        }

        /* FFT */
        fft_256(real, imag);

        /* Power spectrum: |FFT|² */
        float power[256];
        for (int i = 0; i < 256; i++) {
            power[i] = real[i] * real[i] + imag[i] * imag[i];
        }

        /* Apply mel filterbank → 40 mel bins */
        for (int m = 0; m < MEL_BANDS; m++) {
            float mel_energy = 0.0f;
            for (int k = 0; k < 256; k++) {
                mel_energy += s_mel_filters[m][k] * power[k];
            }
            /* Log compression (add small epsilon to avoid log(0)) */
            mel_energy = 10.0f * log10f(mel_energy + 1.0f);

            /* Quantize to uint8 (0-255, mapping 0-80 dB) */
            uint8_t val = (uint8_t)(mel_energy * 3.2f);  /* 80 dB × 3.2 = 256 */
            if (val > 255) val = 255;

            mel_out[frame_idx * MEL_BANDS + m] = val;
        }

        frame_idx++;
    }

    return frame_idx;
}

/* ── On-device wheeze pre-classifier ───────────────────── */
/* Lightweight heuristic: wheeze has characteristic pitch
   between 100-1000 Hz with sustained harmonic structure.
   The full CNN runs on the Hub. */
static uint8_t wheeze_preclassify(const uint8_t *mel_spectrogram, size_t mel_len)
{
    /* Look for sustained energy in low-mid mel bands (100-1000 Hz)
       Mel bands 2-10 cover roughly this range for 40-band filterbank */
    int low_energy = 0;
    int total_energy = 0;

    for (int f = 0; f < MEL_FRAMES; f++) {
        int frame_energy = 0;
        for (int m = 2; m < 12; m++) {
            frame_energy += mel_spectrogram[f * MEL_BANDS + m];
        }
        if (frame_energy > 200) low_energy++;

        for (int m = 0; m < MEL_BANDS; m++) {
            total_energy += mel_spectrogram[f * MEL_BANDS + m];
        }
    }

    /* Wheeze signature: sustained low-freq energy across many frames */
    float sustain_ratio = (float)low_energy / MEL_FRAMES;
    float avg_energy = (float)total_energy / (MEL_FRAMES * MEL_BANDS);

    /* Simple logistic: combine sustain + energy */
    float score = 1.0f / (1.0f + expf(-(sustain_ratio * 4.0f + avg_energy / 50.0f - 3.0f)));

    return (uint8_t)(score * 100.0f);
}

/* ── Process audio frame (called every 2 seconds) ──────── */
int wheeze_process_frame(const int16_t *audio, size_t samples,
                         uint8_t *out_wheeze_prob, audio_feature_t *out_feature)
{
    if (!audio || samples < AUDIO_FRAME_SAMPLES)
        return -1;

    /* Compute mel-spectrogram */
    static uint8_t mel_spectrogram[MEL_BANDS * MEL_FRAMES];
    int frames = wheeze_compute_mel(audio, samples, mel_spectrogram,
                                     sizeof(mel_spectrogram));
    if (frames <= 0)
        return -2;

    /* Run pre-classifier */
    uint8_t prob = wheeze_preclassify(mel_spectrogram, sizeof(mel_spectrogram));

    /* Compute SNR (simplified) */
    float signal_max = 0, noise_floor = 0;
    for (int i = 0; i < 256; i++) {
        float mag = fabsf((float)audio[i]);
        if (mag > signal_max) signal_max = mag;
        noise_floor += mag;
    }
    noise_floor /= 256.0f;
    float snr = 20.0f * log10f((signal_max + 1.0f) / (noise_floor + 1.0f));

    /* Fill output feature */
    if (out_feature) {
        /* Downsample 40 bands → 32 bins for transmission */
        for (int i = 0; i < 32; i++) {
            /* Average groups of mel bins */
            int src_start = i * MEL_BANDS / 32;
            int src_end = (i + 1) * MEL_BANDS / 32;
            uint16_t sum = 0;
            int count = src_end - src_start;
            for (int m = src_start; m < src_end; m++) {
                sum += mel_spectrogram[(MEL_FRAMES - 1) * MEL_BANDS + m];
            }
            out_feature->mel_bins[i] = (uint8_t)(sum / (count > 0 ? count : 1));
        }
        out_feature->wheeze_prob = prob;
        out_feature->snr_db = (uint8_t)snr;
        out_feature->window_ms = AUDIO_FRAME_MS;
    }

    *out_wheeze_prob = prob;

    /* Check if wheeze detected */
    uint32_t now = k_uptime_get_32();
    if (prob >= WHEEZE_PROB_THRESHOLD && (now - s_last_wheeze_ms) > WHEEZE_COOLDOWN_MS) {
        s_last_wheeze_ms = now;
        s_wheeze_count++;

        LOG_INF("Wheeze detected! prob=%u%% (count=%u)", prob, s_wheeze_count);

        if (s_callback) {
            s_callback(prob, out_feature);
        }
    }

    return 0;
}

/* ── Audio capture callback (I²S DMA) ──────────────────── */
void wheeze_on_audio(const int16_t *samples, size_t count)
{
    /* Copy into ring buffer */
    for (size_t i = 0; i < count && s_write_idx < AUDIO_FRAME_SAMPLES; i++) {
        s_audio_ring[s_write_idx++] = samples[i];
    }

    /* When buffer is full, process */
    if (s_write_idx >= AUDIO_FRAME_SAMPLES) {
        uint8_t prob = 0;
        audio_feature_t feature;
        memset(&feature, 0, sizeof(feature));

        wheeze_process_frame(s_audio_ring, AUDIO_FRAME_SAMPLES, &prob, &feature);

        /* Reset ring buffer */
        s_write_idx = 0;
        s_buffer_full = true;
    }
}

/* ── Initialize wheeze detection ───────────────────────── */
int wheeze_init(void)
{
    init_mel_filterbank();
    init_hamming();
    s_write_idx = 0;
    s_wheeze_count = 0;
    s_last_wheeze_ms = 0;
    LOG_INF("Wheeze detector initialized (mel=%d bands, FFT=256)", MEL_BANDS);
    return 0;
}

/* ── Register callback ─────────────────────────────────── */
void wheeze_set_callback(wheeze_callback_t cb)
{
    s_callback = cb;
}

/* ── Get wheeze count ──────────────────────────────────── */
uint8_t wheeze_get_count(void)
{
    return s_wheeze_count;
}