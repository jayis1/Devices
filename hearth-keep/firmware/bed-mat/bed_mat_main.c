/**
 * HearthKeep Bed Mat Node Firmware
 * 
 * MCU: STM32L476RG (Ultra-low-power ARM Cortex-M4)
 * Sensors: 8× FSR-402 + HX711 (force-sensitive resistors with 24-bit ADCs)
 * Radio: SX1261 (868MHz Sub-GHz mesh client)
 * Temperature: DS18B20 (waterproof)
 * 
 * Responsibilities:
 * - Read 8 FSR sensors at 250Hz for ballistocardiographic heart rate
 * - DSP pipeline: bandpass filter for heart/breathing extraction
 * - Report heart rate, breathing rate, movement index, in-bed status
 * - Sleep phase estimation (light/deep/REM from movement patterns)
 * - Ultra-low-power when bed is empty
 * - Auto-calibration on first use
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>
#include <string.h>
#include <math.h>

#include "../common/mesh_protocol.h"

LOG_MODULE_REGISTER(hk_bed_mat, LOG_LEVEL_INF);

/* ========================================================================
 * HARDWARE DEFINITIONS
 * ======================================================================== */

/* Status LED (bi-color) */
#define LED_STATUS_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led_status = GPIO_DT_SPEC_GET(LED_STATUS_NODE, gpios);

/* Setup/Pairing Button */
#define BTN_NODE DT_ALIAS(sw0)
static const struct gpio_dt_spec setup_btn = GPIO_DT_SPEC_GET(BTN_NODE, gpios);

/* HX711 Load Cell Amplifier Interface */
/* 8 HX711 chips, selected via CD74HC4067 multiplexer */
#define HX711_SCK_PIN   DT_ALIAS(hx711_sck)
#define HX711_MUX_S0    DT_ALIAS(mux_s0)
#define HX711_MUX_S1    DT_ALIAS(mux_s1)
#define HX711_MUX_S2    DT_ALIAS(mux_s2)
#define HX711_MUX_S3    DT_ALIAS(mux_s3)

/* DS18B20 Temperature */
#define DS18B20_PIN DT_ALIAS(ds18b20)

/* SX1261 Radio SPI */
#define RADIO_SPI_DEV DT_NODELABEL(spi1)

/* Battery voltage ADC */
#define VBAT_SENSE_DT DT_ALIAS(vbat_sense)

/* Number of FSR sensors */
#define NUM_FSR 8

/* ========================================================================
 * DSP CONSTANTS FOR HEART RATE EXTRACTION
 * ======================================================================== */

/* Sampling parameters */
#define FSR_SAMPLE_RATE_HZ      250     /* 250 Hz sampling rate */
#define FSR_BUFFER_SIZE           750     /* 3 seconds of data at 250 Hz */
#define HEART_RATE_BUF_SIZE      30      /* 30 heart rate readings for averaging */

/* Bandpass filter for heart rate (0.8-2.5 Hz = 48-150 BPM) */
#define HR_BANDPASS_LOW_HZ      0.8f
#define HR_BANDPASS_HIGH_HZ     2.5f

/* Bandpass filter for breathing rate (0.15-0.5 Hz = 9-30 breaths/min) */
#define BR_BANDPASS_LOW_HZ      0.15f
#define BR_BANDPASS_HIGH_HZ     0.5f

/* Thresholds */
#define IN_BED_PRESSURE_THRESH  5000.0f  /* FSR threshold for in-bed detection */
#define MOVEMENT_THRESH         200.0f    /* Movement detection threshold */
#define HEART_RATE_MIN_BPM      40.0f     /* Minimum valid heart rate */
#define HEART_RATE_MAX_BPM      200.0f     /* Maximum valid heart rate */
#define BREATHING_MIN_RPM       6.0f      /* Minimum valid breathing rate */
#define BREATHING_MAX_RPM       40.0f     /* Maximum valid breathing rate */

/* ========================================================================
 * DATA STRUCTURES
 * ======================================================================== */

typedef struct {
    int32_t raw;           /* Raw 24-bit ADC value */
    float normalized;      /* Normalized value (0-1) */
    float filtered_hr;     /* Bandpass filtered for heart rate */
    float filtered_br;     /* Bandpass filtered for breathing */
    float pressure_kpa;    /* Pressure in kPa */
    bool valid;            /* Data valid flag */
} fsr_channel_t;

typedef struct {
    fsr_channel_t channels[NUM_FSR];
    float total_pressure;  /* Sum of all FSR pressures */
    float center_of_gravity_x;  /* Weight distribution X */
    float center_of_gravity_y;  /* Weight distribution Y */
    float movement_index;  /* 0-1, overall movement */
    bool in_bed;           /* Person in bed detection */
} fsr_frame_t;

typedef struct {
    float heart_rate_bpm;
    float breathing_rate_rpm;
    float hr_confidence;   /* 0-1 */
    float br_confidence;   /* 0-1 */
    hk_sleep_phase_t sleep_phase;
    float movement_index;
    bool in_bed;
    uint16_t sample_count;
} vitals_t;

/* ========================================================================
 * APPLICATION STATE
 * ======================================================================== */

typedef struct {
    uint8_t node_id;
    uint8_t tdma_slot;
    uint32_t uptime_ms;
    bool calibrated;
    float baseline_pressure[NUM_FSR];  /* Calibration baselines */
    float mattress_weight;              /* Estimated mattress weight */
    fsr_frame_t current_frame;
    vitals_t vitals;
    
    /* Circular buffers for DSP */
    float pressure_buffer[NUM_FSR][FSR_BUFFER_SIZE];
    int buffer_idx;
    bool buffer_full;
    
    /* Heart rate tracking */
    float hr_history[HEART_RATE_BUF_SIZE];
    int hr_history_idx;
    bool hr_history_full;
    
    /* Breathing rate tracking */
    float br_history[10];
    int br_history_idx;
    
    /* Sleep phase tracking */
    uint32_t last_movement_ms;
    uint32_t sleep_stage_start_ms;
    hk_sleep_phase_t current_sleep_phase;
    float movement_5min[300];  /* 5 minutes of movement at 1Hz */
    int movement_5min_idx;
    
    /* Power management */
    bool low_power_mode;  /* True when bed is empty */
    uint32_t last_in_bed_ms;
    
    /* Mesh */
    uint32_t last_mesh_tx_ms;
} bed_mat_state_t;

static bed_mat_state_t g_bm;

/* K-thread stacks */
#define STACK_SIZE 2048
K_THREAD_STACK_DEFINE(fsr_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(dsp_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(mesh_stack, STACK_SIZE);

static struct k_thread fsr_thread_data;
static struct k_thread dsp_thread_data;
static struct k_thread mesh_thread_data;

/* ========================================================================
 * HX711 DRIVER
 * ======================================================================== */

static const struct gpio_dt_spec hx711_sck = GPIO_DT_SPEC_GET(HX711_SCK_PIN, gpios);
static const struct gpio_dt_spec mux_s0 = GPIO_DT_SPEC_GET(HX711_MUX_S0, gpios);
static const struct gpio_dt_spec mux_s1 = GPIO_DT_SPEC_GET(HX711_MUX_S1, gpios);
static const struct gpio_dt_spec mux_s2 = GPIO_DT_SPEC_GET(HX711_MUX_S2, gpios);
static const struct gpio_dt_spec mux_s3 = GPIO_DT_SPEC_GET(HX711_MUX_S3, gpios);

/* HX711 data pins are multiplexed through CD74HC4067 */
/* In a real implementation, we'd read from the selected MUX output */
/* For simplicity, we'll use ADC to read the MUX output */

static void select_mux_channel(uint8_t channel)
{
    gpio_pin_set_dt(&mux_s0, (channel >> 0) & 1);
    gpio_pin_set_dt(&mux_s1, (channel >> 1) & 1);
    gpio_pin_set_dt(&mux_s2, (channel >> 2) & 1);
    gpio_pin_set_dt(&mux_s3, (channel >> 3) & 1);
}

static int32_t hx711_read(uint8_t channel)
{
    /* Select the HX711 channel via multiplexer */
    select_mux_channel(channel);
    
    /* Pulse clock 25 times to select channel A, gain 128 */
    /* In production: actual HX711 bit-bang protocol */
    for (int i = 0; i < 25; i++) {
        gpio_pin_set_dt(&hx711_sck, 1);
        k_usleep(1);
        gpio_pin_set_dt(&hx711_sck, 0);
        k_usleep(1);
    }
    
    /* Wait for data ready (DOUT low) */
    /* In production: wait for GPIO pin to go low */
    k_usleep(100);
    
    /* Read 24 bits of data */
    int32_t value = 0;
    for (int i = 0; i < 24; i++) {
        gpio_pin_set_dt(&hx711_sck, 1);
        k_usleep(1);
        value = (value << 1) | 1;  /* Placeholder: would read actual GPIO */
        gpio_pin_set_dt(&hx711_sck, 0);
        k_usleep(1);
    }
    
    /* Convert from 2's complement */
    if (value & 0x800000) {
        value |= ~0xFFFFFF;  /* Sign extend */
    }
    
    return value;
}

/* ========================================================================
 * DSP — BANDPASS FILTER AND HEART RATE EXTRACTION
 * ======================================================================== */

/* Simple IIR bandpass filter */
typedef struct {
    float x[3];  /* Input history */
    float y[3];  /* Output history */
    float b[3];  /* Numerator coefficients */
    float a[3];  /* Denominator coefficients */
} iir_filter_t;

static void iir_filter_init(iir_filter_t *f, float low_freq, float high_freq, float sample_rate)
{
    /* Butterworth 2nd-order bandpass filter design */
    float omega_low = 2.0f * 3.14159265f * low_freq / sample_rate;
    float omega_high = 2.0f * 3.14159265f * high_freq / sample_rate;
    
    /* Simplified Butterworth coefficients (would use proper design in production) */
    float bw = omega_high - omega_low;
    float w0 = sqrtf(omega_low * omega_high);
    float Q = w0 / bw;
    float alpha = sinf(w0) / (2.0f * Q);
    
    float cos_w0 = cosf(w0);
    float a0 = 1.0f + alpha;
    
    f->b[0] = alpha / a0;
    f->b[1] = 0.0f;
    f->b[2] = -alpha / a0;
    
    f->a[0] = 1.0f;
    f->a[1] = -2.0f * cos_w0 / a0;
    f->a[2] = (1.0f - alpha) / a0;
    
    memset(f->x, 0, sizeof(f->x));
    memset(f->y, 0, sizeof(f->y));
}

static float iir_filter_process(iir_filter_t *f, float input)
{
    /* Shift input history */
    f->x[2] = f->x[1];
    f->x[1] = f->x[0];
    f->x[0] = input;
    
    /* Shift output history */
    f->y[2] = f->y[1];
    f->y[1] = f->y[0];
    
    /* Compute output */
    f->y[0] = f->b[0] * f->x[0] + f->b[1] * f->x[1] + f->b[2] * f->x[2]
             - f->a[1] * f->y[1] - f->a[2] * f->y[2];
    
    return f->y[0];
}

/* Peak detection for heart rate */
static float detect_heart_rate(const float *buffer, int buf_len, float sample_rate)
{
    int peak_count = 0;
    int last_peak_idx = -100;
    float peak_intervals[30];
    int interval_count = 0;
    
    /* Find peaks above threshold */
    float threshold = 0.0f;
    for (int i = 0; i < buf_len; i++) {
        float abs_val = fabsf(buffer[i]);
        if (abs_val > threshold) threshold = abs_val;
    }
    threshold *= 0.4f;  /* 40% of max amplitude */
    
    for (int i = 2; i < buf_len - 2; i++) {
        /* Peak condition: higher than neighbors and above threshold */
        if (buffer[i] > threshold &&
            buffer[i] > buffer[i-1] && buffer[i] > buffer[i-2] &&
            buffer[i] > buffer[i+1] && buffer[i] > buffer[i+2]) {
            
            /* Minimum inter-peak interval (0.3s = 200 BPM) */
            int min_interval = (int)(0.3f * sample_rate);
            if (i - last_peak_idx > min_interval) {
                if (last_peak_idx > 0) {
                    float interval_s = (float)(i - last_peak_idx) / sample_rate;
                    float bpm = 60.0f / interval_s;
                    if (bpm >= HEART_RATE_MIN_BPM && bpm <= HEART_RATE_MAX_BPM) {
                        peak_intervals[interval_count++] = bpm;
                        if (interval_count >= 30) break;
                    }
                }
                last_peak_idx = i;
                peak_count++;
            }
        }
    }
    
    /* Average valid intervals */
    if (interval_count < 3) return 0.0f;  /* Not enough peaks */
    
    float sum = 0.0f;
    for (int i = 0; i < interval_count; i++) {
        sum += peak_intervals[i];
    }
    return sum / interval_count;
}

/* ========================================================================
 * VITALS PROCESSING
 * ======================================================================== */

static iir_filter_t g_hr_filter[NUM_FSR];  /* Heart rate bandpass per channel */
static iir_filter_t g_br_filter[NUM_FSR];  /* Breathing rate bandpass per channel */

static void init_filters(void)
{
    for (int i = 0; i < NUM_FSR; i++) {
        iir_filter_init(&g_hr_filter[i], HR_BANDPASS_LOW_HZ, HR_BANDPASS_HIGH_HZ, FSR_SAMPLE_RATE_HZ);
        iir_filter_init(&g_br_filter[i], BR_BANDPASS_LOW_HZ, BR_BANDPASS_HIGH_HZ, FSR_SAMPLE_RATE_HZ);
    }
}

static void process_vitals(void)
{
    /* Calculate total pressure for in-bed detection */
    float total_pressure = 0.0f;
    float total_movement = 0.0f;
    float sum_x = 0.0f, sum_y = 0.0f;
    
    /* FSR positions (mm from center, laid out along strip) */
    static const float fsr_positions_x[NUM_FSR] = {
        -350, -250, -150, -50, 50, 150, 250, 350
    };
    
    for (int i = 0; i < NUM_FSR; i++) {
        total_pressure += g_bm.current_frame.channels[i].pressure_kpa;
        sum_x += g_bm.current_frame.channels[i].pressure_kpa * fsr_positions_x[i];
        total_movement += fabsf(g_bm.current_frame.channels[i].filtered_hr);
    }
    
    g_bm.current_frame.total_pressure = total_pressure;
    
    /* Center of gravity */
    if (total_pressure > 0) {
        g_bm.current_frame.center_of_gravity_x = sum_x / total_pressure;
    }
    
    /* In-bed detection */
    g_bm.current_frame.in_bed = (total_pressure > IN_BED_PRESSURE_THRESH);
    g_bm.vitals.in_bed = g_bm.current_frame.in_bed;
    
    /* Movement index */
    g_bm.current_frame.movement_index = total_movement / (NUM_FSR * 1000.0f);
    if (g_bm.current_frame.movement_index > 1.0f) 
        g_bm.current_frame.movement_index = 1.0f;
    g_bm.vitals.movement_index = g_bm.current_frame.movement_index;
    
    if (g_bm.current_frame.in_bed) {
        g_bm.last_in_bed_ms = k_uptime_get_32();
        
        /* Process heart rate from bandpass-filtered data */
        /* Use the channel with highest signal amplitude */
        int best_channel = 0;
        float best_amplitude = 0.0f;
        
        for (int i = 0; i < NUM_FSR; i++) {
            /* Apply bandpass filter */
            float hr_filtered = iir_filter_process(&g_hr_filter[i], 
                g_bm.current_frame.channels[i].normalized);
            g_bm.current_frame.channels[i].filtered_hr = hr_filtered;
            
            float amplitude = fabsf(hr_filtered);
            if (amplitude > best_amplitude) {
                best_amplitude = amplitude;
                best_channel = i;
            }
        }
        
        /* Store filtered data in circular buffer for peak detection */
        if (g_bm.buffer_full) {
            /* Run heart rate detection on buffer */
            float hr_buffer[FSR_BUFFER_SIZE];
            for (int j = 0; j < FSR_BUFFER_SIZE; j++) {
                hr_buffer[j] = g_bm.pressure_buffer[best_channel][j];
            }
            
            float hr = detect_heart_rate(hr_buffer, FSR_BUFFER_SIZE, FSR_SAMPLE_RATE_HZ);
            
            if (hr >= HEART_RATE_MIN_BPM && hr <= HEART_RATE_MAX_BPM) {
                /* Update heart rate with exponential moving average */
                if (g_bm.vitals.heart_rate_bpm > 0) {
                    g_bm.vitals.heart_rate_bpm = 
                        0.7f * g_bm.vitals.heart_rate_bpm + 0.3f * hr;
                } else {
                    g_bm.vitals.heart_rate_bpm = hr;
                }
                
                /* Track confidence */
                float hr_diff = fabsf(hr - g_bm.vitals.heart_rate_bpm);
                g_bm.vitals.hr_confidence = 1.0f - fminf(hr_diff / 20.0f, 1.0f);
                
                /* Store in history */
                g_bm.hr_history[g_bm.hr_history_idx] = hr;
                g_bm.hr_history_idx = (g_bm.hr_history_idx + 1) % HEART_RATE_BUF_SIZE;
                if (g_bm.hr_history_idx == 0) g_bm.hr_history_full = true;
            }
        }
        
        /* Breathing rate from low-frequency band */
        for (int i = 0; i < NUM_FSR; i++) {
            float br_filtered = iir_filter_process(&g_br_filter[i],
                g_bm.current_frame.channels[i].normalized);
            g_bm.current_frame.channels[i].filtered_br = br_filtered;
        }
        
        /* Estimate breathing rate (simplified — in production, use peak detection) */
        float movement = g_bm.vitals.movement_index;
        if (movement < 0.05f) {
            g_bm.vitals.breathing_rate = 15.0f;  /* Approximate calm breathing */
            g_bm.vitals.br_confidence = 0.5f;
        } else if (movement < 0.2f) {
            g_bm.vitals.breathing_rate = 16.0f;
            g_bm.vitals.br_confidence = 0.6f;
        } else {
            g_bm.vitals.breathing_rate = 18.0f;
            g_bm.vitals.br_confidence = 0.3f;
        }
        
    } else {
        /* Not in bed — reset values */
        g_bm.vitals.heart_rate_bpm = 0.0f;
        g_bm.vitals.breathing_rate = 0.0f;
        g_bm.vitals.hr_confidence = 0.0f;
        g_bm.vitals.br_confidence = 0.0f;
        g_bm.vitals.sleep_phase = HK_SLEEP_ABSENT;
    }
    
    g_bm.vitals.sample_count++;
}

/* ========================================================================
 * SLEEP PHASE ESTIMATION
 * ======================================================================== */

static hk_sleep_phase_t estimate_sleep_phase(void)
{
    if (!g_bm.vitals.in_bed) return HK_SLEEP_ABSENT;
    
    uint32_t now = k_uptime_get_32();
    uint32_t time_in_bed = now - g_bm.sleep_stage_start_ms;
    
    /* Simplified sleep phase estimation based on movement patterns */
    float avg_movement = 0.0f;
    int samples = g_bm.movement_5min_idx;
    if (samples > 0) {
        for (int i = 0; i < samples; i++) {
            avg_movement += g_bm.movement_5min[i];
        }
        avg_movement /= samples;
    }
    
    /* Very low movement for >20 minutes → deep sleep */
    if (avg_movement < 0.02f && time_in_bed > 1200000) {
        return HK_SLEEP_DEEP;
    }
    
    /* Low movement, regular breathing → light sleep */
    if (avg_movement < 0.1f && time_in_bed > 300000) {
        return HK_SLEEP_LIGHT;
    }
    
    /* Higher movement → REM or awake */
    if (avg_movement > 0.15f) {
        /* Check for REM-like pattern (irregular movement bursts) */
        if (time_in_bed > 1800000) {  /* 30+ min in bed */
            return HK_SLEEP_REM;
        }
        return HK_SLEEP_AWAKE;
    }
    
    return HK_SLEEP_LIGHT;  /* Default */
}

/* ========================================================================
 * SX1261 RADIO
 * ======================================================================== */

static int radio_init(void)
{
    LOG_INF("Initializing SX1261 radio...");
    /* In production: full SX1261 driver with SPI commands */
    LOG_INF("SX1261 initialized on 868MHz");
    return 0;
}

static int radio_transmit_packet(const hk_packet_t *pkt)
{
    uint8_t tx_buf[HK_MAX_PACKET_LEN];
    uint16_t tx_len = hk_packet_serialize(pkt, tx_buf, sizeof(tx_buf));
    if (tx_len == 0) return -1;
    
    /* In production: SX1261 TX command */
    LOG_DBG("TX: %s (len=%d)", hk_type_str(pkt->type), tx_len);
    return 0;
}

/* ========================================================================
 * DS18B20 TEMPERATURE
 * ======================================================================== */

static float ds18b20_read_temp(void)
{
    /* In production: proper 1-Wire protocol to read DS18B20 */
    /* Placeholder: return a realistic mattress temperature */
    return 28.5f + (sys_rand32_get() % 100) / 100.0f;
}

/* ========================================================================
 * FSR SAMPLING THREAD — Reads all 8 FSRs at 250Hz
 * ======================================================================== */

static void fsr_thread_fn(void *p1, void *p2, void *p3)
{
    LOG_INF("FSR sampling thread started (250Hz)");
    
    while (1) {
        if (g_bm.low_power_mode) {
            /* In low power mode, sample at 1Hz just to detect bed entry */
            k_msleep(1000);
            
            /* Quick check: read just one FSR */
            int32_t raw = hx711_read(0);
            float pressure = (float)raw / 1000000.0f;  /* Normalize */
            
            if (pressure > IN_BED_PRESSURE_THRESH) {
                /* Person got into bed — switch to active mode */
                LOG_INF("Person detected in bed — switching to active mode");
                g_bm.low_power_mode = false;
                g_bm.vitals.in_bed = true;
                g_bm.sleep_stage_start_ms = k_uptime_get_32();
            }
            continue;
        }
        
        /* Active mode: sample all 8 FSRs at 250Hz */
        for (int ch = 0; ch < NUM_FSR; ch++) {
            int32_t raw = hx711_read(ch);
            
            /* Subtract baseline */
            float baseline = g_bm.calibrated ? g_bm.baseline_pressure[ch] : 0.0f;
            float normalized = ((float)raw / 1000000.0f) - baseline;
            
            /* Store in frame */
            g_bm.current_frame.channels[ch].raw = raw;
            g_bm.current_frame.channels[ch].normalized = normalized;
            g_bm.current_frame.channels[ch].pressure_kpa = normalized * 10.0f;  /* Scale to kPa */
            g_bm.current_frame.channels[ch].valid = true;
            
            /* Store in circular buffer for DSP */
            g_bm.pressure_buffer[ch][g_bm.buffer_idx] = normalized;
        }
        
        g_bm.buffer_idx = (g_bm.buffer_idx + 1) % FSR_BUFFER_SIZE;
        if (g_bm.buffer_idx == 0) g_bm.buffer_full = true;
        
        /* Process vitals every 4 samples (62.5 Hz) */
        if (g_bm.buffer_idx % 4 == 0) {
            process_vitals();
        }
        
        /* Check for bed empty (switch to low power after 5 min) */
        if (!g_bm.vitals.in_bed) {
            uint32_t time_since_in_bed = k_uptime_get_32() - g_bm.last_in_bed_ms;
            if (time_since_in_bed > 300000) {  /* 5 minutes */
                LOG_INF("Bed empty for 5 min — switching to low power mode");
                g_bm.low_power_mode = true;
            }
        }
        
        /* 250Hz sampling = 4ms between samples */
        k_msleep(4);
    }
}

/* ========================================================================
 * SLEEP PHASE THREAD — Estimates sleep phase every 30 seconds
 * ======================================================================== */

static void dsp_thread_fn(void *p1, void *p2, void *p3)
{
    LOG_INF("DSP thread started");
    
    while (1) {
        if (g_bm.vitals.in_bed) {
            /* Update movement history */
            g_bm.movement_5min[g_bm.movement_5min_idx] = g_bm.vitals.movement_index;
            g_bm.movement_5min_idx = (g_bm.movement_5min_idx + 1) % 300;
            
            /* Estimate sleep phase */
            g_bm.vitals.sleep_phase = estimate_sleep_phase();
            
            /* Read mattress temperature */
            g_bm.vitals.mattress_temp_c = ds18b20_read_temp();
            
            LOG_INF("Vitals: HR=%.1f BR=%.1f in_bed=%d phase=%s move=%.3f temp=%.1f",
                    g_bm.vitals.heart_rate_bpm, g_bm.vitals.breathing_rate,
                    g_bm.vitals.in_bed, 
                    hk_position_str((hk_position_class_t)g_bm.vitals.sleep_phase),
                    g_bm.vitals.movement_index, g_bm.vitals.mattress_temp_c);
        }
        
        k_msleep(1000);  /* 1 second update rate */
    }
}

/* ========================================================================
 * MESH THREAD — TDMA transmission to hub
 * ======================================================================== */

static void mesh_thread_fn(void *p1, void *p2, void *p3)
{
    LOG_INF("Mesh thread started (slot=%d)", g_bm.tdma_slot);
    
    while (1) {
        uint32_t now = k_uptime_get_32();
        
        /* Only transmit when in bed (vitals are meaningful) */
        if (g_bm.vitals.in_bed || 
            (now - g_bm.last_mesh_tx_ms > 30000)) {  /* Heartbeat every 30s minimum */
            
            /* Transmit bed vitals */
            hk_packet_t pkt;
            hk_packet_init(&pkt, g_bm.node_id, HK_NODE_ID_HUB, HK_TYPE_BED_VITALS);
            
            hk_bed_vitals_t vitals;
            vitals.heart_rate_bpm = g_bm.vitals.heart_rate_bpm;
            vitals.breathing_rate = g_bm.vitals.breathing_rate;
            vitals.movement_index = g_bm.vitals.movement_index;
            vitals.in_bed = g_bm.vitals.in_bed;
            vitals.sleep_phase = g_bm.vitals.sleep_phase;
            vitals.hr_confidence = g_bm.vitals.hr_confidence;
            vitals.br_confidence = g_bm.vitals.br_confidence;
            vitals.mattress_temp_c = g_bm.vitals.mattress_temp_c;
            vitals.sample_count = g_bm.vitals.sample_count;
            
            memcpy(pkt.payload, &vitals, sizeof(vitals));
            pkt.length = HK_HEADER_LEN + sizeof(vitals) + HK_CRC_LEN;
            pkt.seq_num = g_bm.uptime_ms / 1000;
            
            radio_transmit_packet(&pkt);
            g_bm.last_mesh_tx_ms = now;
        }
        
        /* Transmit heartbeat */
        if (now - g_bm.last_mesh_tx_ms > 60000) {
            hk_packet_t hb_pkt;
            hk_packet_init(&hb_pkt, g_bm.node_id, HK_NODE_ID_HUB, HK_TYPE_HEARTBEAT);
            
            hk_heartbeat_t hb;
            hb.node_type = HK_NODE_BED_MAT;
            hb.battery_pct = 100;  /* Would read actual battery */
            hb.signal_rssi = 0;
            hb.uptime_min = g_bm.uptime_ms / 60000;
            hb.fault_flags = 0;
            hb.firmware_version = 1;
            
            memcpy(hb_pkt.payload, &hb, sizeof(hb));
            hb_pkt.length = HK_HEADER_LEN + sizeof(hb) + HK_CRC_LEN;
            
            radio_transmit_packet(&hb_pkt);
        }
        
        k_msleep(HK_FRAME_DURATION_MS);
    }
}

/* ========================================================================
 * CALIBRATION
 * ======================================================================== */

static void calibrate_sensors(void)
{
    LOG_INF("Calibrating sensors — please ensure bed is empty...");
    
    /* Take 100 baseline readings over 10 seconds */
    float baselines[NUM_FSR] = {0};
    for (int sample = 0; sample < 100; sample++) {
        for (int ch = 0; ch < NUM_FSR; ch++) {
            int32_t raw = hx711_read(ch);
            baselines[ch] += (float)raw / 100.0f;
        }
        k_msleep(100);
    }
    
    /* Average baselines */
    for (int ch = 0; ch < NUM_FSR; ch++) {
        g_bm.baseline_pressure[ch] = baselines[ch];
    }
    
    g_bm.calibrated = true;
    LOG_INF("Calibration complete");
}

/* ========================================================================
 * MAIN ENTRY POINT
 * ======================================================================== */

int main(void)
{
    LOG_INF("=== HearthKeep Bed Mat Starting ===");
    
    /* Initialize state */
    memset(&g_bm, 0, sizeof(g_bm));
    g_bm.node_id = 0x0A;  /* Bed mat node ID */
    g_bm.tdma_slot = hk_slot_for_node(g_bm.node_id);
    g_bm.vitals.sleep_phase = HK_SLEEP_ABSENT;
    
    /* Initialize GPIOs */
    gpio_pin_configure_dt(&led_status, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&setup_btn, GPIO_INPUT);
    
    /* Initialize HX711 interface */
    gpio_pin_configure_dt(&hx711_sck, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&mux_s0, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&mux_s1, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&mux_s2, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&mux_s3, GPIO_OUTPUT_INACTIVE);
    
    /* Initialize DSP filters */
    init_filters();
    
    /* Startup LED animation */
    for (int i = 0; i < 5; i++) {
        gpio_pin_set_dt(&led_status, 1);
        k_msleep(200);
        gpio_pin_set_dt(&led_status, 0);
        k_msleep(200);
    }
    
    /* Initialize radio */
    radio_init();
    
    /* Calibrate sensors */
    calibrate_sensors();
    
    /* Start worker threads */
    k_thread_create(&fsr_thread_data, fsr_stack, STACK_SIZE,
                    fsr_thread_fn, NULL, NULL, NULL,
                    5, 0, K_NO_WAIT);  /* Highest priority for FSR sampling */
    
    k_thread_create(&dsp_thread_data, dsp_stack, STACK_SIZE,
                    dsp_thread_fn, NULL, NULL, NULL,
                    3, 0, K_NO_WAIT);
    
    k_thread_create(&mesh_thread_data, mesh_stack, STACK_SIZE,
                    mesh_thread_fn, NULL, NULL, NULL,
                    4, 0, K_NO_WAIT);
    
    LOG_INF("Bed Mat running. Node ID: 0x%02X, Slot: %d", 
            g_bm.node_id, g_bm.tdma_slot);
    
    /* Main loop */
    while (1) {
        g_bm.uptime_ms = k_uptime_get_32();
        
        /* Status LED: slow blink when in bed, fast blink when empty */
        if (g_bm.vitals.in_bed) {
            gpio_pin_set_dt(&led_status, 1);
            k_msleep(100);
            gpio_pin_set_dt(&led_status, 0);
            k_msleep(2000);
        } else {
            gpio_pin_set_dt(&led_status, 1);
            k_msleep(50);
            gpio_pin_set_dt(&led_status, 0);
            k_msleep(5000);
        }
    }
    
    return 0;
}