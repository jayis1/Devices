/**
 * @file spl.c
 * @brief Sound Pressure Level (SPL) calculation implementation.
 *
 * Implements A/C/Z-weighted SPL measurement, 1/3-octave spectrum
 * analysis, and sound dose calculation per IEC 61672 and OSHA standards.
 */

#include "spl.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ── A-Weighting IIR Filter Coefficients (16kHz sample rate) ────────── */
/* Designed using bilinear transform from analog A-weighting prototype.
 * Cascaded 3 biquads for 6th-order approximation. */

/* Stage 1: High-pass at ~500Hz */
static const spl_iir_coeff_t a_weight_stage1 = {
    .b0 =  0.3024f,
    .b1 =  0.0000f,
    .b2 = -0.3024f,
    .a1 = -0.9990f,
    .a2 =  0.3953f,
};

/* Stage 2: High-pass at ~100Hz */
static const spl_iir_coeff_t a_weight_stage2 = {
    .b0 =  0.9468f,
    .b1 = -1.8936f,
    .b2 =  0.9468f,
    .a1 = -1.8899f,
    .a2 =  0.8973f,
};

/* Stage 3: Band-boost around 2-4kHz */
static const spl_iir_coeff_t a_weight_stage3 = {
    .b0 =  1.0000f,
    .b1 = -0.6221f,
    .b2 =  0.1164f,
    .a1 =  0.6221f,
    .a2 = -0.1164f,
};

/* ── C-Weighting IIR Filter Coefficients (16kHz sample rate) ────────── */

/* Stage 1: High-pass at ~20Hz */
static const spl_iir_coeff_t c_weight_stage1 = {
    .b0 =  0.9856f,
    .b1 = -1.9712f,
    .b2 =  0.9856f,
    .a1 = -1.9722f,
    .a2 =  0.9726f,
};

/* Stage 2: Low-pass at ~20kHz */
static const spl_iir_coeff_t c_weight_stage2 = {
    .b0 =  0.0656f,
    .b1 =  0.1311f,
    .b2 =  0.0656f,
    .a1 = -1.1978f,
    .a2 =  0.4601f,
};

/* ── Private Functions ──────────────────────────────────────────────── */

static float iir_biquad_process(const spl_iir_coeff_t *coeff,
                                  spl_iir_state_t *state,
                                  float input)
{
    float output = coeff->b0 * input
                 + coeff->b1 * state->x1
                 + coeff->b2 * state->x2
                 - coeff->a1 * state->y1
                 - coeff->a2 * state->y2;

    state->x2 = state->x1;
    state->x1 = input;
    state->y2 = state->y1;
    state->y1 = output;

    return output;
}

static float compute_rms(const float *samples, size_t num_samples)
{
    double sum_sq = 0.0;
    for (size_t i = 0; i < num_samples; i++) {
        sum_sq += (double)samples[i] * (double)samples[i];
    }
    return (float)sqrtf(sum_sq / (double)num_samples);
}

static float rms_to_db(float rms)
{
    if (rms <= 0.0f) return -120.0f;
    return 20.0f * log10f(rms);
}

/* ── Public Functions ─────────────────────────────────────────────────── */

int spl_init(spl_calculator_t *calc)
{
    if (!calc) return -1;
    memset(calc, 0, sizeof(spl_calculator_t));

    /* Default sensitivity for SPH0645LM4H-6: -26 dBFS @ 94 dB SPL */
    calc->mic_sensitivity_dbfs = -26.0f;
    calc->calibration_offset_db = 0.0f;

    /* Initialize A-weighting filter */
    calc->a_weight_coeff[0] = a_weight_stage1;
    calc->a_weight_coeff[1] = a_weight_stage2;
    calc->a_weight_coeff[2] = a_weight_stage3;

    /* Initialize C-weighting filter */
    calc->c_weight_coeff[0] = c_weight_stage1;
    calc->c_weight_coeff[1] = c_weight_stage2;

    /* Enable statistics and spectrum by default */
    calc->stats_enabled = true;
    calc->stats_enabled = true;
    calc->spectrum_enabled = true;

    /* Reset statistics */
    spl_reset_stats(&calc->stats);

    return 0;
}

void spl_calibrate(spl_calculator_t *calc, float reference_db)
{
    if (!calc) return;
    /* Adjust calibration offset so current reading = reference */
    calc->calibration_offset_db = reference_db - calc->stats.leq_dba;
}

int spl_process(spl_calculator_t *calc, const int16_t *samples,
                size_t num_samples, spl_result_t *result)
{
    if (!calc || !samples || !result) return -1;

    /* Convert int16 samples to float (-1.0 to 1.0) */
    float *float_samples = (float *)malloc(num_samples * sizeof(float));
    float *a_weighted = (float *)malloc(num_samples * sizeof(float));
    float *c_weighted = (float *)malloc(num_samples * sizeof(float));
    if (!float_samples || !a_weighted || !c_weighted) {
        free(float_samples); free(a_weighted); free(c_weighted);
        return -2;
    }

    /* Normalize to -1.0 to 1.0 */
    for (size_t i = 0; i < num_samples; i++) {
        float_samples[i] = (float)samples[i] / 32768.0f;
    }

    /* Copy for A-weighting */
    memcpy(a_weighted, float_samples, num_samples * sizeof(float));
    memcpy(c_weighted, float_samples, num_samples * sizeof(float));

    /* Apply A-weighting */
    spl_apply_a_weighting(calc, a_weighted, num_samples);

    /* Apply C-weighting */
    spl_apply_c_weighting(calc, c_weighted, num_samples);

    /* Compute RMS for each weighting */
    float rms_z = compute_rms(float_samples, num_samples);
    float rms_a = compute_rms(a_weighted, num_samples);
    float rms_c = compute_rms(c_weighted, num_samples);

    /* Convert to dB SPL */
    float dbfs_z = rms_to_db(rms_z);
    float dbfs_a = rms_to_db(rms_a);
    float dbfs_c = rms_to_db(rms_c);

    /* Apply mic sensitivity and calibration */
    float ref_dbfs = calc->mic_sensitivity_dbfs;  /* -26 dBFS */
    float offset = 94.0f - ref_dbfs + calc->calibration_offset_db;

    result->spl_dbz = dbfs_z + offset;
    result->spl_dba = dbfs_a + offset;
    result->spl_dbc = dbfs_c + offset;

    /* Compute spectrum if enabled */
    if (calc->spectrum_enabled) {
        spl_compute_spectrum(calc, float_samples, num_samples,
                            result->spectrum_dba);
    }

    /* Update statistics */
    if (calc->stats_enabled) {
        spl_update_stats(&calc->stats, result->spl_dba);
        result->spl_leq_dba = calc->stats.leq_dba;
        result->spl_min_dba = calc->stats.lmin_dba;
        result->spl_max_dba = calc->stats.lmax_dba;
        result->spl_peak_dba = calc->stats.peak_dba;
    }

    result->timestamp_ms = 0;  /* Caller should set this */

    free(float_samples);
    free(a_weighted);
    free(c_weighted);
    return 0;
}

float spl_rms_to_dba(float rms, float sensitivity_dbfs, float offset_db)
{
    if (rms <= 0.0f) return -120.0f;
    float dbfs = 20.0f * log10f(rms);
    float spl = dbfs + (94.0f - sensitivity_dbfs) + offset_db;
    return spl;
}

void spl_apply_a_weighting(spl_calculator_t *calc, float *samples,
                            size_t num_samples)
{
    for (size_t i = 0; i < num_samples; i++) {
        float sample = samples[i];
        for (int stage = 0; stage < 3; stage++) {
            sample = iir_biquad_process(&calc->a_weight_coeff[stage],
                                        &calc->a_weight_state[stage],
                                        sample);
        }
        samples[i] = sample;
    }
}

void spl_apply_c_weighting(spl_calculator_t *calc, float *samples,
                            size_t num_samples)
{
    for (size_t i = 0; i < num_samples; i++) {
        float sample = samples[i];
        for (int stage = 0; stage < 2; stage++) {
            sample = iir_biquad_process(&calc->c_weight_coeff[stage],
                                        &calc->c_weight_state[stage],
                                        sample);
        }
        samples[i] = sample;
    }
}

void spl_compute_spectrum(spl_calculator_t *calc, const float *samples,
                           size_t num_samples, float spectrum[SPL_NUM_BANDS])
{
    /* Simple 1/3-octave analysis using weighting corrections.
     * For a production system, use proper band-pass filters.
     * Here we apply the A-weighting corrections from the lookup table. */

    /* Compute overall RMS */
    float rms = compute_rms(samples, num_samples);
    float overall_db = rms_to_db(rms) + (94.0f - calc->mic_sensitivity_dbfs)
                       + calc->calibration_offset_db;

    /* Approximate spectrum using A-weighting corrections.
     * In production, this would use proper 1/3-octave bandpass filters. */
    for (int i = 0; i < SPL_NUM_BANDS; i++) {
        /* Simplified: spread overall level across bands, then apply weighting.
         * Real implementation would use parallel bandpass filters. */
        float band_db = overall_db - 10.0f * log10f((float)SPL_NUM_BANDS)
                       + spl_a_weight_db[i];
        spectrum[i] = band_db;
    }
}

void spl_reset_stats(spl_stats_t *stats)
{
    if (!stats) return;
    memset(stats, 0, sizeof(spl_stats_t));
    stats->lmax_dba = -120.0f;
    stats->lmin_dba = 200.0f;
    stats->peak_dba = -120.0f;
    stats->leq_dba = 0.0f;
}

void spl_update_stats(spl_stats_t *stats, float spl_dba)
{
    if (!stats) return;

    stats->sample_count++;
    stats->sum_spl += spl_dba;
    stats->sum_sq_spl += spl_dba * spl_dba;

    /* Leq: energy average */
    stats->leq_dba = 10.0f * log10f(stats->sum_sq_spl / stats->sample_count);

    /* Lmax and Lmin */
    if (spl_dba > stats->lmax_dba) stats->lmax_dba = spl_dba;
    if (spl_dba < stats->lmin_dba) stats->lmin_dba = spl_dba;

    /* Peak */
    if (spl_dba > stats->peak_dba) stats->peak_dba = spl_dba;
}

void spl_compute_percentiles(spl_stats_t *stats, const float *history,
                              size_t count)
{
    if (!stats || !history || count == 0) return;

    /* Simple sort for percentile calculation (production would use
     * a more efficient method like partial sort) */
    float *sorted = (float *)malloc(count * sizeof(float));
    if (!sorted) return;

    memcpy(sorted, history, count * sizeof(float));

    /* Insertion sort (adequate for small arrays) */
    for (size_t i = 1; i < count; i++) {
        float key = sorted[i];
        size_t j = i;
        while (j > 0 && sorted[j - 1] > key) {
            sorted[j] = sorted[j - 1];
            j--;
        }
        sorted[j] = key;
    }

    /* Percentiles */
    stats->l10_dba = sorted[(size_t)(count * 0.10f)];
    stats->l50_dba = sorted[(size_t)(count * 0.50f)];
    stats->l90_dba = sorted[(size_t)(count * 0.90f)];

    free(sorted);
}

float spl_calculate_dose(float spl_dba, float duration_hours)
{
    /* 3 dB exchange rate (ISO standard, more conservative than OSHA's 5 dB)
     * Reference: 85 dB(A) for 8 hours = 100% dose */
    float reference_level = 85.0f;
    float reference_hours = 8.0f;
    float exchange_rate = 3.0f;

    float allowed_hours = reference_hours *
        powf(2.0f, (reference_level - spl_dba) / exchange_rate);

    if (allowed_hours <= 0.0f) return 999.9f;  /* Infinite exposure */

    float dose_pct = (duration_hours / allowed_hours) * 100.0f;
    return dose_pct;
}

float spl_max_exposure_hours(float spl_dba)
{
    float reference_level = 85.0f;
    float reference_hours = 8.0f;
    float exchange_rate = 3.0f;

    float allowed = reference_hours *
        powf(2.0f, (reference_level - spl_dba) / exchange_rate);

    return (allowed > 0.0f) ? allowed : 0.0f;
}