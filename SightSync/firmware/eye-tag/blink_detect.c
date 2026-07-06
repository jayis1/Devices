/**
 * SightSync Eye Tag — Blink Detection Implementation
 *
 * IR LED (940 nm) + photodiode reflectance method.
 * Detects eyelid closures by observing reflectance dips
 * of 10-40% over 100-400 ms duration.
 *
 * License: MIT
 */

#include "blink_detect.h"
#include <Arduino.h>

/* ── Configuration ────────────────────────────────────────────────── */

#define SAMPLE_HZ        50
#define WINDOW_SAMPLES   100    /* 2 seconds at 50 Hz */
#define BLINK_MIN_DUR    5      /* 100 ms (5 samples @ 50 Hz) */
#define BLINK_MAX_DUR    20     /* 400 ms (20 samples @ 50 Hz) */
#define THRESHOLD_PCT    15     /* 15% dip = blink candidate */
#define IR_LED_PWM       128    /* 50% duty cycle */

/* ── State ────────────────────────────────────────────────────────── */

static uint8_t s_ir_pin;
static uint8_t s_pd_pin;
static uint8_t s_mode = 0;  /* 0=work, 1=rest, 2=child, 3=sleep */

static uint16_t s_samples[WINDOW_SAMPLES];
static uint8_t  s_sample_idx = 0;
static bool     s_buffer_full = false;

static uint16_t s_baseline = 0;       /* running baseline reflectance */
static uint16_t s_blink_count_2s = 0;  /* blinks in last 2 seconds */
static uint8_t  s_blink_rate_bpm = 0;  /* computed bpm */
static uint8_t  s_confidence = 0;
static uint8_t  s_ir_amplitude = 0;

/* Blink detection state machine */
typedef enum {
    BLINK_STATE_IDLE,
    BLINK_STATE_DIP_DETECTED,
    BLINK_STATE_RECOVERING,
} blink_state_t;

static blink_state_t s_blink_state = BLINK_STATE_IDLE;
static uint8_t s_dip_duration = 0;
static uint16_t s_dip_min_val = 0xFFFF;
static uint16_t s_pre_dip_val = 0;

/* Blue-light dose accumulator (mJ/cm²) */
static uint16_t s_blue_dose = 0;

/* ── Adaptive sampling rates ──────────────────────────────────────── */

static uint8_t get_sample_interval_ms(void)
{
    switch (s_mode) {
    case 0: return 20;   /* work: 50 Hz */
    case 1: return 40;   /* rest: 25 Hz */
    case 2: return 20;   /* child: 50 Hz */
    case 3: return 200;  /* sleep: 5 Hz (minimal) */
    default: return 20;
    }
}

/* ── Init ─────────────────────────────────────────────────────────── */

void blink_detect_init(uint8_t ir_led_pin, uint8_t photodiode_pin)
{
    s_ir_pin = ir_led_pin;
    s_pd_pin = photodiode_pin;

    pinMode(ir_led_pin, OUTPUT);
    analogWrite(ir_led_pin, IR_LED_PWM);

    /* Initialize baseline with first 50 samples */
    uint32_t sum = 0;
    for (int i = 0; i < 50; i++) {
        sum += analogRead(photodiode_pin);
        delay(20);
    }
    s_baseline = sum / 50;
}

/* ── Sample + detect ──────────────────────────────────────────────── */

void blink_detect_sample(void)
{
    /* Read photodiode (10-bit ADC on nRF52840, 0-1023) */
    uint16_t val = analogRead(s_pd_pin);
    s_ir_amplitude = (uint8_t)(val >> 2);  /* scale to 0-255 */

    /* Store in circular buffer */
    s_samples[s_sample_idx] = val;
    s_sample_idx = (s_sample_idx + 1) % WINDOW_SAMPLES;
    if (s_sample_idx == 0) s_buffer_full = true;

    /* Update baseline (exponential moving average) */
    s_baseline = (s_baseline * 0.95f) + (val * 0.05f);

    /* Blink detection state machine */
    uint16_t threshold = s_baseline * (100 - THRESHOLD_PCT) / 100;

    switch (s_blink_state) {
    case BLINK_STATE_IDLE:
        if (val < threshold) {
            s_blink_state = BLINK_STATE_DIP_DETECTED;
            s_dip_duration = 1;
            s_dip_min_val = val;
            s_pre_dip_val = s_baseline;
        }
        break;

    case BLINK_STATE_DIP_DETECTED:
        s_dip_duration++;
        if (val < s_dip_min_val) s_dip_min_val = val;

        /* Check if recovering */
        if (val > (s_baseline * 0.85f)) {
            /* Blink complete — validate */
            if (s_dip_duration >= BLINK_MIN_DUR && s_dip_duration <= BLINK_MAX_DUR) {
                /* Valid blink: dip was 100-400 ms */
                uint16_t dip_depth = s_pre_dip_val - s_dip_min_val;
                uint16_t dip_pct = (dip_depth * 100) / s_pre_dip_val;
                if (dip_pct >= THRESHOLD_PCT && dip_pct <= 60) {
                    s_blink_count_2s++;
                }
            }
            s_blink_state = BLINK_STATE_RECOVERING;
        } else if (s_dip_duration > BLINK_MAX_DUR) {
            /* Too long — not a blink (head movement, etc.) */
            s_blink_state = BLINK_STATE_IDLE;
        }
        break;

    case BLINK_STATE_RECOVERING:
        if (val > (s_baseline * 0.9f)) {
            s_blink_state = BLINK_STATE_IDLE;
        }
        break;
    }

    /* Compute blink rate every 2 seconds (when buffer is full) */
    static uint16_t last_blink_count = 0;
    static uint32_t last_rate_time = 0;
    uint32_t now = millis();
    if (now - last_rate_time >= 2000) {
        /* Count blinks in last 10 seconds (extrapolated from 2s window) */
        uint16_t blinks_this_window = s_blink_count_2s;
        if (s_buffer_full) {
            /* bpm = blinks_in_2s × 30 (2s → 60s / 2s = 30) */
            s_blink_rate_bpm = blinks_this_window * 30;

            /* Confidence based on IR amplitude stability */
            uint16_t amp_variance = 0;
            for (int i = 0; i < 10; i++) {
                int16_t diff = (int16_t)s_samples[i] - s_baseline;
                amp_variance += abs(diff);
            }
            if (amp_variance < 50) {
                s_confidence = 90;  /* good signal */
            } else if (amp_variance < 150) {
                s_confidence = 70;
            } else {
                s_confidence = 40;
            }
        }
        last_blink_count = s_blink_count_2s;
        s_blink_count_2s = 0;
        last_rate_time = now;
    }
}

/* ── Getters ──────────────────────────────────────────────────────── */

uint8_t blink_detect_get_bpm(void)        { return s_blink_rate_bpm; }
uint8_t blink_detect_get_confidence(void) { return s_confidence; }
uint8_t blink_detect_get_quality(void) {
    if (s_confidence > 80) return 2;  /* good */
    if (s_confidence > 50) return 1;  /* fair */
    return 0;                         /* poor */
}
uint8_t blink_detect_get_ir_amplitude(void) { return s_ir_amplitude; }

uint8_t blink_detect_get_battery_pct(void)
{
    /* Read battery voltage via SAADC */
    uint16_t vbat_raw = analogRead(PIN_VBAT);
    /* CR2032: 3.0V new, 2.0V dead
     * ADC: 0-1023 for 0-3.6V (with 1/3 divider on nRF52840)
     * battery_pct = (vbat - 2.0) / 1.0 × 100
     */
    float vbat = (vbat_raw / 1023.0f) * 3.6f * 3.0f;  /* ×3 for divider */
    float pct = (vbat - 2.0f) / 1.0f * 100.0f;
    if (pct > 100) pct = 100;
    if (pct < 0) pct = 0;
    return (uint8_t)pct;
}

void blink_detect_set_mode(uint8_t mode)
{
    s_mode = mode;
}

void blink_detect_accumulate_blue_dose(uint16_t ch0, uint16_t ch1)
{
    /* ch0 = visible+IR, ch1 = IR only.
     * Blue component ≈ ch0 - ch1 (simplified).
     * Dose rate ≈ blue × 0.001 mW/cm² per count.
     * Accumulate over 10-second intervals.
     */
    if (ch0 > ch1) {
        uint16_t blue = ch0 - ch1;
        s_blue_dose += blue / 1000;  /* simplified accumulation */
    }
}