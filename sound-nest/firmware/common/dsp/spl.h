/**
 * @file spl.h
 * @brief Sound Pressure Level (SPL) calculation utilities.
 *
 * Implements A-weighted, C-weighted, and Z-weighted (flat) SPL
 * measurement from raw microphone samples. Complies with IEC 61672.
 */

#ifndef SPL_H
#define SPL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ── Configuration ──────────────────────────────────────────────────── */

#define SPL_SAMPLE_RATE       16000   /* Hz */
#define SPL_WINDOW_MS          125    /* 125ms Fast time constant */
#define SPL_WINDOW_SAMPLES    (SPL_SAMPLE_RATE * SPL_WINDOW_MS / 1000)
#define SPL_SLOW_WINDOW_MS    1000   /* 1s Slow time constant */
#define SPL_SLOW_SAMPLES      (SPL_SAMPLE_RATE * SPL_SLOW_WINDOW_MS / 1000)

/* Number of 1/3-octave bands (20Hz to 20kHz) */
#define SPL_NUM_BANDS         32

/* ── SPL Weighting Types ───────────────────────────────────────────── */

typedef enum {
    SPL_WEIGHTING_Z = 0,  /* Flat/no weighting */
    SPL_WEIGHTING_A = 1,  /* A-weighting (approximates human ear) */
    SPL_WEIGHTING_C = 2,  /* C-weighting (flat above 31.5Hz) */
} spl_weighting_t;

/* ── SPL Result Structure ──────────────────────────────────────────── */

typedef struct {
    float spl_dba;           /* A-weighted SPL in dB */
    float spl_dbc;           /* C-weighted SPL in dB */
    float spl_dbz;           /* Z-weighted (flat) SPL in dB */
    float spl_min_dba;       /* Minimum A-SPL in measurement window */
    float spl_max_dba;       /* Maximum A-SPL in measurement window */
    float spl_peak_dba;      /* Peak A-SPL */
    float spl_leq_dba;       /* Equivalent continuous A-SPL */
    float spectrum_dba[SPL_NUM_BANDS]; /* 1/3-octave spectrum */
    uint32_t timestamp_ms;   /* Measurement timestamp */
} spl_result_t;

/* ── SPL Statistics (accumulated over time) ─────────────────────────── */

typedef struct {
    float leq_dba;           /* Time-averaged equivalent level */
    float lmax_dba;          /* Maximum SPL */
    float lmin_dba;          /* Minimum SPL */
    float l10_dba;           /* Level exceeded 10% of time */
    float l50_dba;           /* Level exceeded 50% of time */
    float l90_dba;           /* Level exceeded 90% of time */
    float peak_dba;          /* Peak level */
    uint32_t sample_count;   /* Number of samples in statistics */
    float sum_spl;           /* Running sum for Leq */
    float sum_sq_spl;        /* Running sum of squares for L10/L50/L90 */
} spl_stats_t;

/* ── A-Weighting Filter Coefficients ────────────────────────────────── */

/**
 * Second-order IIR A-weighting filter coefficients.
 * Designed for 16kHz sample rate.
 * Reference: IEC 61672-1
 */
typedef struct {
    /* Numerator coefficients (b0, b1, b2) */
    float b0, b1, b2;
    /* Denominator coefficients (a1, a2) */
    float a1, a2;
} spl_iir_coeff_t;

/* Filter state for cascaded biquad */
typedef struct {
    float x1, x2;  /* Input history */
    float y1, y2;  /* Output history */
} spl_iir_state_t;

/* ── SPL Calculator (stateful) ──────────────────────────────────────── */

typedef struct {
    /* Calibration */
    float mic_sensitivity_dbfs;  /* Mic sensitivity in dBFS @ 94dB SPL */
    float calibration_offset_db;  /* Calibration offset in dB */

    /* A-weighting IIR filter (cascaded biquads) */
    spl_iir_coeff_t a_weight_coeff[3];  /* 3 cascaded biquads */
    spl_iir_state_t a_weight_state[3];

    /* C-weighting IIR filter */
    spl_iir_coeff_t c_weight_coeff[2];  /* 2 cascaded biquads */
    spl_iir_state_t c_weight_state[2];

    /* Running statistics */
    spl_stats_t stats;
    float spl_history[100];  /* Circular buffer for percentile calc */
    uint16_t history_idx;
    uint16_t history_count;

    /* 1/3-octave band filter states */
    spl_iir_state_t band_state[SPL_NUM_BANDS][2]; /* 2 biquads per band */

    /* Configuration */
    bool stats_enabled;
    bool spectrum_enabled;
} spl_calculator_t;

/* ── Public Functions ───────────────────────────────────────────────── */

/**
 * Initialize SPL calculator with default calibration.
 * @param calc Pointer to SPL calculator instance
 * @return 0 on success, negative on error
 */
int spl_init(spl_calculator_t *calc);

/**
 * Calibrate SPL calculator with a known reference level.
 * @param calc Pointer to SPL calculator instance
 * @param reference_db SPL of reference sound (typically 94 dB)
 */
void spl_calibrate(spl_calculator_t *calc, float reference_db);

/**
 * Process a buffer of audio samples and compute SPL.
 * @param calc Pointer to SPL calculator instance
 * @param samples Audio samples (16-bit signed, 16kHz)
 * @param num_samples Number of samples in buffer
 * @param result Output SPL result
 * @return 0 on success, negative on error
 */
int spl_process(spl_calculator_t *calc, const int16_t *samples,
                size_t num_samples, spl_result_t *result);

/**
 * Compute A-weighted SPL from raw RMS.
 * @param rms Raw RMS value
 * @param sensitivity_dbfs Mic sensitivity in dBFS @ 94dB SPL
 * @param offset_db Calibration offset in dB
 * @return A-weighted SPL in dB
 */
float spl_rms_to_dba(float rms, float sensitivity_dbfs, float offset_db);

/**
 * Apply A-weighting filter to audio samples in-place.
 * @param calc Pointer to SPL calculator (contains filter state)
 * @param samples Audio samples (float, -1.0 to 1.0)
 * @param num_samples Number of samples
 */
void spl_apply_a_weighting(spl_calculator_t *calc, float *samples,
                            size_t num_samples);

/**
 * Apply C-weighting filter to audio samples in-place.
 * @param calc Pointer to SPL calculator (contains filter state)
 * @param samples Audio samples (float, -1.0 to 1.0)
 * @param num_samples Number of samples
 */
void spl_apply_c_weighting(spl_calculator_t *calc, float *samples,
                            size_t num_samples);

/**
 * Compute 1/3-octave spectrum from audio samples.
 * @param calc Pointer to SPL calculator (contains band filters)
 * @param samples Audio samples (float, -1.0 to 1.0)
 * @param num_samples Number of samples
 * @param spectrum Output array of SPL_NUM_BANDS floats (dB per band)
 */
void spl_compute_spectrum(spl_calculator_t *calc, const float *samples,
                           size_t num_samples, float spectrum[SPL_NUM_BANDS]);

/**
 * Reset SPL statistics.
 * @param stats Pointer to statistics structure
 */
void spl_reset_stats(spl_stats_t *stats);

/**
 * Update SPL statistics with a new measurement.
 * @param stats Pointer to statistics structure
 * @param spl_dba New A-weighted SPL measurement
 */
void spl_update_stats(spl_stats_t *stats, float spl_dba);

/**
 * Compute percentile levels (L10, L50, L90) from history.
 * @param stats Pointer to statistics structure
 * @param history Array of historical SPL values
 * @param count Number of values in history
 */
void spl_compute_percentiles(spl_stats_t *stats, const float *history,
                              size_t count);

/**
 * Calculate daily sound dose percentage.
 * Uses 3 dB exchange rate (ISO standard).
 * @param spl_dba Average SPL in dB(A)
 * @param duration_hours Exposure duration in hours
 * @return Dose percentage (100% = maximum daily exposure)
 */
float spl_calculate_dose(float spl_dba, float duration_hours);

/**
 * Calculate maximum allowed exposure time at a given SPL.
 * @param spl_dba Sound pressure level in dB(A)
 * @return Maximum exposure time in hours (8h reference at 85 dB(A))
 */
float spl_max_exposure_hours(float spl_dba);

/* ── 1/3-Octave Band Center Frequencies ─────────────────────────────── */

static const float spl_band_centers[SPL_NUM_BANDS] = {
    20.0f, 25.0f, 31.5f, 40.0f, 50.0f, 63.0f, 80.0f, 100.0f,
    125.0f, 160.0f, 200.0f, 250.0f, 315.0f, 400.0f, 500.0f, 630.0f,
    800.0f, 1000.0f, 1250.0f, 1600.0f, 2000.0f, 2500.0f, 3150.0f, 4000.0f,
    5000.0f, 6300.0f, 8000.0f, 10000.0f, 12500.0f, 16000.0f, 20000.0f, 25000.0f
};

/* ── A-Weighting Correction (dB) at Standard Frequencies ─────────────── */

static const float spl_a_weight_db[SPL_NUM_BANDS] = {
    -50.5f, -44.7f, -39.4f, -34.6f, -30.2f, -26.2f, -22.5f, -19.1f,
    -16.1f, -13.4f, -10.9f, -8.6f,  -6.6f,  -4.8f,  -3.2f,  -1.9f,
    -0.8f,   0.0f,   0.6f,   1.0f,   1.2f,   1.3f,   1.2f,   1.0f,
     0.5f,  -0.1f,  -1.1f,  -2.5f,  -4.3f,  -6.6f,  -9.3f,  -12.4f
};

/* ── C-Weighting Correction (dB) at Standard Frequencies ─────────────── */

static const float spl_c_weight_db[SPL_NUM_BANDS] = {
    -6.2f, -4.4f, -3.0f, -2.0f, -1.3f, -0.8f, -0.5f, -0.3f,
    -0.2f, -0.1f, -0.1f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,
     0.0f,  0.0f,  0.0f, -0.1f, -0.2f, -0.3f, -0.5f, -0.8f,
    -1.3f, -2.0f, -3.0f, -4.4f, -6.2f, -8.5f, -11.2f, -14.2f
};

#endif /* SPL_H */