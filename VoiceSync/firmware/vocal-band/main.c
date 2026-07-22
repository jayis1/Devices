/*
 * VoiceSync — Vocal Band Firmware (Wearable Throat Band)
 * nRF52840, nRF Connect SDK v2.x
 *
 * The Vocal Band is a wearable throat-band device that continuously
 * monitors vocal fold vibrations through a contact microphone,
 * extracts acoustic features (F0, jitter, shimmer, HNR), tracks
 * neck posture via IMU, monitors skin temperature (inflammation proxy)
 * and PPG heart rate/HRV (stress affects voice), and transmits
 * telemetry to the Hub via BLE 5.0 every 30 seconds.
 *
 * Build: west build -b nrf52840dk_nrf52840
 *
 * Feature extraction:
 *   F0      — Autocorrelation pitch detection (70-600 Hz)
 *   Jitter  — Cycle-to-cycle F0 perturbation (%)
 *   Shimmer — Cycle-to-cycle amplitude perturbation (%)
 *   HNR     — Harmonics-to-noise ratio (dB)
 *   Phonation % — Voiced speech percentage in 5s window
 *   Intensity   — RMS amplitude in dB SPL (calibrated)
 *   Pitch range — Semitone span (max-min F0)
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/sys/printk.h>

#include "../common/protocol.h"
#include "../common/config.h"

#define LOG_TAG "VocalBand"

/* === Audio capture buffer === */
#define AUDIO_SAMPLE_RATE 16000
#define AUDIO_WINDOW_MS   5000
#define AUDIO_SAMPLES     (AUDIO_SAMPLE_RATE * AUDIO_WINDOW_MS / 1000) /* 80000 */
#define AUDIO_BLOCK_SIZE  256

static int16_t audio_buffer[AUDIO_BLOCK_SIZE * 4]; /* Rolling buffer */
static uint16_t audio_block_idx = 0;

/* === Feature extraction state === */
static float g_f0_current = 0.0f;
static float g_jitter_current = 0.0f;
static float g_shimmer_current = 0.0f;
static float g_hnr_current = 0.0f;
static uint8_t g_phonation_pct = 0;
static uint8_t g_intensity_db = 0;
static uint16_t g_pitch_range_deci = 0;

/* === IMU state (neck angle) === */
static int16_t g_neck_angle_deci = 0;  /* ×0.1 degrees */
static float g_neck_forward_time_s = 0.0f;

/* === PPG state === */
static uint8_t g_heart_rate = 0;
static uint8_t g_hrv_rmssd = 0;
static uint8_t g_stress_level = 0;

/* === Skin temp state === */
static float g_skin_temp_c = 0.0f;

/* === BLE state === */
static uint8_t g_node_id = 0xFF;
static uint16_t g_msg_seq = 0;
static uint8_t g_joined = 0;

/* === F0 Detection (Autocorrelation) ===
 * Finds fundamental frequency by autocorrelation in 70-600 Hz range.
 * Operates on a block of audio samples.
 */
static float detect_f0(const int16_t *samples, int n)
{
    int min_lag = AUDIO_SAMPLE_RATE / 600;  /* 600 Hz */
    int max_lag = AUDIO_SAMPLE_RATE / 70;    /* 70 Hz */

    float best_corr = 0.0f;
    int best_lag = 0;

    for (int lag = min_lag; lag <= max_lag; lag++) {
        float corr = 0.0f;
        for (int i = 0; i < n - lag; i++) {
            corr += (float)samples[i] * (float)samples[i + lag];
        }
        corr /= (n - lag);

        if (corr > best_corr) {
            best_corr = corr;
            best_lag = lag;
        }
    }

    if (best_lag > 0) {
        return (float)AUDIO_SAMPLE_RATE / best_lag;
    }
    return 0.0f;
}

/* === Jitter Calculation ===
 * Jitter(%) = mean(|F0_i - F0_{i-1}|) / mean(F0) × 100
 * Computed over consecutive F0 estimates.
 */
#define JITTER_HISTORY 10
static float f0_history[JITTER_HISTORY];
static int f0_history_idx = 0;

static float compute_jitter(void)
{
    if (f0_history_idx < 3) return 0.0f;

    float sum_diff = 0.0f;
    float sum_f0 = 0.0f;
    int n = f0_history_idx;

    for (int i = 1; i < n; i++) {
        sum_diff += fabsf(f0_history[i] - f0_history[i-1]);
        sum_f0 += f0_history[i];
    }
    sum_f0 += f0_history[0];

    float mean_f0 = sum_f0 / n;
    if (mean_f0 < 1.0f) return 0.0f;

    return (sum_diff / (n - 1)) / mean_f0 * 100.0f;
}

/* === Shimmer Calculation ===
 * Shimmer(%) = mean(|A_i - A_{i-1}|) / mean(A) × 100
 * Computed over consecutive cycle amplitudes.
 */
static float compute_shimmer(const int16_t *samples, int n)
{
    /* Approximate: compute peak amplitude per block */
    int block = n / 10;
    float amps[10];
    for (int i = 0; i < 10; i++) {
        int16_t max_val = 0;
        for (int j = 0; j < block; j++) {
            int16_t val = samples[i * block + j];
            if (val < 0) val = -val;
            if (val > max_val) max_val = val;
        }
        amps[i] = (float)max_val;
    }

    float sum_diff = 0.0f;
    float sum_amp = 0.0f;
    for (int i = 1; i < 10; i++) {
        sum_diff += fabsf(amps[i] - amps[i-1]);
        sum_amp += amps[i];
    }
    sum_amp += amps[0];

    if (sum_amp < 1.0f) return 0.0f;
    return (sum_diff / 9.0f) / (sum_amp / 10.0f) * 100.0f;
}

/* === HNR (Harmonics-to-Noise Ratio) ===
 * Ratio of harmonic energy to noise energy in dB.
 * Approximate: compare energy at F0 multiples to energy between harmonics.
 */
static float compute_hnr(const int16_t *samples, int n, float f0)
{
    if (f0 < 70.0f || f0 > 600.0f) return 0.0f;

    /* Simple approximation: SNR based on autocorrelation peak */
    int lag = (int)(AUDIO_SAMPLE_RATE / f0 + 0.5f);
    if (lag >= n) return 0.0f;

    float corr_at_f0 = 0.0f;
    float total_energy = 0.0f;
    for (int i = 0; i < n - lag; i++) {
        corr_at_f0 += (float)samples[i] * (float)samples[i + lag];
        total_energy += (float)samples[i] * (float)samples[i];
    }

    if (total_energy < 1.0f) return 0.0f;
    float snr = corr_at_f0 / total_energy;
    if (snr <= 0.0f) return 0.0f;

    return 10.0f * log10f(snr / (1.0f - snr));
}

/* === Phonation Detection ===
 * Detects voiced speech by checking if RMS energy exceeds threshold.
 * Returns percentage of voiced frames in the window.
 */
static uint8_t compute_phonation_pct(const int16_t *samples, int n)
{
    int voiced_frames = 0;
    int total_frames = 0;
    int frame_size = AUDIO_SAMPLE_RATE / 100; /* 10 ms frames */

    for (int i = 0; i < n - frame_size; i += frame_size) {
        float rms = 0.0f;
        for (int j = 0; j < frame_size; j++) {
            float v = (float)samples[i + j] / 32768.0f;
            rms += v * v;
        }
        rms = sqrtf(rms / frame_size);
        total_frames++;
        if (rms > 0.02f) {  /* Threshold for voiced speech */
            voiced_frames++;
        }
    }

    if (total_frames == 0) return 0;
    return (uint8_t)(voiced_frames * 100 / total_frames);
}

/* === Voice Intensity (dB SPL) === */
static uint8_t compute_intensity_db(const int16_t *samples, int n)
{
    float rms = 0.0f;
    for (int i = 0; i < n; i++) {
        float v = (float)samples[i] / 32768.0f;
        rms += v * v;
    }
    rms = sqrtf(rms / n);
    if (rms < 0.0001f) return 0;

    /* Convert to dB SPL (approximate, calibrated against reference) */
    float db = 20.0f * log10f(rms) + 90.0f; /* Offset for calibration */
    if (db < 40.0f) db = 40.0f;
    if (db > 120.0f) db = 120.0f;
    return (uint8_t)(db - 40.0f); /* Offset by 40 for protocol */
}

/* === Feature Extraction Pipeline === */
static void extract_features(void)
{
    int n = AUDIO_BLOCK_SIZE * 4;
    const int16_t *samples = audio_buffer;

    /* Detect F0 */
    float f0 = detect_f0(samples, n);
    g_f0_current = f0;

    /* Update F0 history for jitter */
    if (f0 > 0) {
        f0_history[f0_history_idx++] = f0;
        if (f0_history_idx >= JITTER_HISTORY) f0_history_idx = 0;
    }

    /* Compute jitter and shimmer */
    g_jitter_current = compute_jitter();
    g_shimmer_current = compute_shimmer(samples, n);

    /* Compute HNR */
    g_hnr_current = compute_hnr(samples, n, f0);

    /* Compute phonation percentage */
    g_phonation_pct = compute_phonation_pct(samples, n);

    /* Compute intensity */
    g_intensity_db = compute_intensity_db(samples, n);

    /* Pitch range (simplified: track over longer window in production) */
    static float f0_min = 600.0f, f0_max = 70.0f;
    if (f0 > 0) {
        if (f0 < f0_min) f0_min = f0;
        if (f0 > f0_max) f0_max = f0;
        if (f0_max > f0_min) {
            g_pitch_range_deci = (uint16_t)(12.0f * log2f(f0_max / f0_min) * 10.0f);
        }
    }

    printk(LOG_TAG ": F0=%.1f Hz jitter=%.2f%% shimmer=%.2f%% HNR=%.1f dB "
           "phonation=%d%% intensity=%d dB range=%.1f st\n",
           g_f0_current, g_jitter_current, g_shimmer_current,
           g_hnr_current, g_phonation_pct, g_intensity_db + 40,
           g_pitch_range_deci / 10.0f);
}

/* === IMU: Neck Angle === */
static void read_imu(void)
{
    /* In production: read LSM6DS3TR-C via I²C
     * Compute neck angle from accelerometer:
     *   angle = atan2(ay, sqrt(ax^2 + az^2)) × 180/PI
     * Positive = forward tilt
     */
    /* Stub: read actual sensor */
    float ax = 0.0f, ay = 0.0f, az = 1.0f; /* Placeholder: upright = 1g on Z */
    float angle = atan2f(ay, sqrtf(ax*ax + az*az)) * 180.0f / 3.14159f;
    g_neck_angle_deci = (int16_t)(angle * 10.0f);

    /* Track sustained forward posture */
    if (angle > NECK_FORWARD_DEG) {
        g_neck_forward_time_s += (float)VOCAL_BAND_SAMPLE_MS / 1000.0f;
    } else {
        g_neck_forward_time_s = 0.0f;
    }
}

/* === TMP117: Skin Temperature === */
static void read_skin_temp(void)
{
    /* In production: read TMP117 via I²C at address 0x48 */
    /* Stub: 35.0°C normal vocal cord skin temp */
    g_skin_temp_c = 35.0f;
}

/* === PPG: MAX30102 Heart Rate/HRV === */
static void read_ppg(void)
{
    /* In production: read MAX30102 via I²C, compute HR and HRV
     * from PPG peak detection (30s window)
     */
    /* Stub values */
    g_heart_rate = 72;
    g_hrv_rmssd = 45;
    /* Stress level from HRV: low HRV = high stress */
    if (g_hrv_rmssd < 20) g_stress_level = 80;
    else if (g_hrv_rmssd < 40) g_stress_level = 50;
    else g_stress_level = 20;
}

/* === Battery Voltage === */
static uint8_t read_battery_mv(void)
{
    /* In production: read ADC on P0.15 */
    /* Stub: 3.90V = 390 (×0.01V) */
    return 390;
}

/* === Build and send telemetry via BLE === */
static void send_telemetry(void)
{
    vs_message_t msg;

    vs_build_vocal_band_telem(&msg, g_node_id, g_msg_seq++,
        read_battery_mv(),
        (uint16_t)(g_f0_current * 10.0f),
        (uint16_t)(g_jitter_current * 100.0f),
        (uint16_t)(g_shimmer_current * 100.0f),
        (int8_t)g_hnr_current,
        g_phonation_pct,
        g_intensity_db,
        g_pitch_range_deci,
        g_neck_angle_deci,
        (uint16_t)((g_skin_temp_c - 20.0f) * 100.0f),
        g_heart_rate,
        g_hrv_rmssd,
        g_stress_level,
        0);  /* RSSI via BLE */

    /* In production: send via BLE GATT notification to Hub */
    uint8_t buf[VS_MAX_MSG];
    size_t len = vs_encode(&msg, buf, sizeof(buf));
    printk(LOG_TAG ": BLE TX %d bytes (seq=%d)\n", (int)len, g_msg_seq - 1);

    /* Check for vocal rest needed */
    if (g_phonation_pct > 80) {
        vs_message_t alert;
        vs_build_alert(&alert, g_node_id, g_msg_seq++,
                       VS_ALERT_VOCAL_REST, 2,
                       (uint8_t[]){g_phonation_pct}, 1);
        /* Send alert via BLE */
    }

    /* Check for poor posture sustained */
    if (g_neck_forward_time_s > NECK_POSTURE_SUSTAIN_S) {
        vs_message_t alert;
        vs_build_alert(&alert, g_node_id, g_msg_seq++,
                       VS_ALERT_POOR_POSTURE, 1,
                       (uint8_t[]){(uint8_t)(g_neck_forward_time_s)}, 1);
        /* Send alert via BLE */
    }
}

/* === Audio Capture Task === */
static void audio_task(void *arg)
{
    while (1) {
        /* In production: capture audio from NAU88C22 codec via I²S
         * Read 256-sample blocks at 16 kHz
         * Store in rolling buffer
         */
        /* Stub: fill with silence for compilation */
        for (int i = 0; i < AUDIO_BLOCK_SIZE; i++) {
            audio_buffer[audio_block_idx++] = 0;
            if (audio_block_idx >= AUDIO_BLOCK_SIZE * 4) {
                audio_block_idx = 0;
            }
        }

        /* Extract features every full buffer cycle */
        if (audio_block_idx == 0) {
            extract_features();
        }

        k_msleep(AUDIO_BLOCK_SIZE * 1000 / AUDIO_SAMPLE_RATE);
    }
}

/* === Sensor Task === */
static void sensor_task(void *arg)
{
    while (1) {
        read_imu();
        read_skin_temp();
        read_ppg();
        k_msleep(VOCAL_BAND_SAMPLE_MS);
    }
}

/* === BLE TX Task === */
static void ble_tx_task(void *arg)
{
    while (1) {
        if (g_joined) {
            send_telemetry();
        }
        k_msleep(VOCAL_BAND_TX_MS);
    }
}

/* === Main === */
int main(void)
{
    printk(LOG_TAG ": VoiceSync Vocal Band starting...\n");

    /* In production:
     * 1. Initialize I²C bus for IMU, TMP117, PPG, codec config
     * 2. Initialize I²S for audio codec (NAU88C22)
     * 3. Initialize BLE SoftDevice (central role, connect to Hub)
     * 4. Join VoiceSync network via BLE
     * 5. Start audio capture, sensor, and TX tasks
     */

    g_node_id = 1; /* Assigned by Hub in production */
    g_joined = 1;  /* Simplified */

    /* Start tasks */
    k_thread_create(NULL, audio_stack, 4096, audio_task, NULL, NULL, NULL,
                    5, 0, K_NO_WAIT);
    k_thread_create(NULL, sensor_stack, 2048, sensor_task, NULL, NULL, NULL,
                    4, 0, K_NO_WAIT);
    k_thread_create(NULL, ble_stack, 2048, ble_tx_task, NULL, NULL, NULL,
                    3, 0, K_NO_WAIT);

    printk(LOG_TAG ": VoiceSync Vocal Band running\n");
    return 0;
}

/* Thread stacks (in production: K_THREAD_STACK_DEFINE) */
__attribute__((aligned(4)))
static uint8_t audio_stack[4096];
__attribute__((aligned(4)))
static uint8_t sensor_stack[2048];
__attribute__((aligned(4)))
static uint8_t ble_stack[2048];