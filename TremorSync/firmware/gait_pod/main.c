/*
 * TremorSync Gait Pod Firmware
 * Target: nRF52840 QFAA + SX1262 Sub-GHz
 *
 * Shoe-mounted IMU + dual FSR for freezing-of-gait detection, stride analysis,
 * festination detection. Sub-GHz 868 MHz TDMA mesh to Hub.
 *
 * Freezing Index = Power(3–8 Hz) / [Power(0–3 Hz) + Power(3–8 Hz)]
 * FI > 0.5 for > 0.5 s = FOG episode (Moore et al., 2007)
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "nrf.h"
#include "nrf_log.h"
#include "nrf_delay.h"
#include "nrf_drv_spi.h"
#include "nrf_drv_gpiote.h"
#include "nrfx_gpiote.h"
#include "nrf_drv_saadc.h"
#include "app_error.h"
#include "app_timer.h"
#include "nrf_pwr_mgmt.h"

#include "../common/protocol.h"
#include "../common/mesh.h"

#define TAG "GAIT_POD"

/* ---- Pin definitions ---- */
#define PIN_SPI_CS_IMU  4
#define PIN_SPI_SCK     5
#define PIN_SPI_MISO    6
#define PIN_SPI_MOSI    7
#define PIN_IMU_INT     8
#define PIN_SX_CS       9
#define PIN_SX_DIO1     10
#define PIN_SX_BUSY     11
#define PIN_SX_RESET    12
#define PIN_HEEL_FSR    26  /* ADC input A0 */
#define PIN_TOE_FSR     27  /* ADC input A1 */
#define PIN_BAT_SENSE   28
#define PIN_LED         29
#define PIN_BTN_PAIR    30

/* ---- Sampling ---- */
#define SAMPLE_RATE_HZ  100
#define GAIT_WINDOW     256   /* 2.56 s window at 100 Hz */
#define FSR_THRESHOLD   200   /* ADC counts for contact detection */

/* ---- ICM-42688-P registers ---- */
#define IMU_REG_ACCEL_DATA 0x1F
#define IMU_REG_GYRO_DATA  0x25
#define IMU_REG_PWR_MGMT0  0x4C

/* ---- Gait state ---- */
static float g_ax[GAIT_WINDOW], g_az[GAIT_WINDOW];
static int   g_sample_idx = 0;
static bool  g_window_ready = false;

static float g_stride_length = 0.0f;
static float g_cadence       = 0.0f;
static float g_freeze_index  = 0.0f;
static bool  g_fog_detected  = false;
static bool  g_festination   = false;
static uint8_t g_battery_pct = 100;

/* ---- Heel/toe FSR readings ---- */
static uint16_t g_heel_pressure = 0;
static uint16_t g_toe_pressure  = 0;
static bool     g_heel_strike   = false;
static bool     g_toe_off       = false;
static int      g_step_count    = 0;

/* ---- SPI for IMU + SX1262 (shared bus) ---- */
static nrf_drv_spi_t g_spi = NRF_DRV_SPI_INSTANCE(0);

static void spi_init(void)
{
    nrf_drv_spi_config_t cfg = NRF_DRV_SPI_DEFAULT_CONFIG;
    cfg.sck_pin  = PIN_SPI_SCK;
    cfg.mosi_pin = PIN_SPI_MOSI;
    cfg.miso_pin = PIN_SPI_MISO;
    cfg.frequency = NRF_DRV_SPI_FREQ_10M;
    cfg.mode = NRF_DRV_SPI_MODE_0;
    nrf_drv_spi_init(&g_spi, &cfg, NULL, NULL);

    nrf_gpio_cfg_output(PIN_SPI_CS_IMU);
    nrf_gpio_cfg_output(PIN_SX_CS);
    nrf_gpio_pin_set(PIN_SPI_CS_IMU);
    nrf_gpio_pin_set(PIN_SX_CS);
}

/* ---- IMU ---- */
static void imu_init(void)
{
    nrf_gpio_pin_clear(PIN_SPI_CS_IMU);
    uint8_t tx[2] = {IMU_REG_PWR_MGMT0 & 0x7F, 0x0F};
    nrf_drv_spi_transfer(&g_spi, tx, 2, NULL, 0);
    nrf_gpio_pin_set(PIN_SPI_CS_IMU);
    NRF_LOG_INFO("ICM-42688-P initialized @ 100 Hz");
}

static void imu_read_sample(float *ax, float *ay, float *az)
{
    uint8_t buf[6];
    nrf_gpio_pin_clear(PIN_SPI_CS_IMU);
    uint8_t tx = IMU_REG_ACCEL_DATA | 0x80;
    nrf_drv_spi_transfer(&g_spi, &tx, 1, buf, 6);
    nrf_gpio_pin_set(PIN_SPI_CS_IMU);

    int16_t ax_raw = (buf[0] << 8) | buf[1];
    int16_t ay_raw = (buf[2] << 8) | buf[3];
    int16_t az_raw = (buf[4] << 8) | buf[5];
    const float scale = 8.0f / 32768.0f * 9.80665f;
    *ax = ax_raw * scale;
    *ay = ay_raw * scale;
    *az = az_raw * scale;
}

/* ---- SAADC for FSR ---- */
static void saadc_init(void)
{
    nrf_drv_saadc_config_t cfg = NRF_DRV_SAADC_DEFAULT_CONFIG;
    nrf_drv_saadc_init(&cfg, NULL);
    nrf_saadc_channel_config_t heel_cfg = {
        .resistor_p = NRF_SAADC_RESISTOR_PULLDOWN,
        .resistor_n = NRF_SAADC_RESISTOR_DISABLED,
        .gain       = NRF_SAADC_GAIN1_4,
        .reference  = NRF_SAADC_REFERENCE_VDD4,
        .acq_time   = NRF_SAADC_ACQTIME_10us,
        .mode       = NRF_SAADC_MODE_SINGLE_ENDED,
        .burst      = NRF_SAADC_BURST_DISABLED,
        .pin_p      = (nrf_saadc_input_t)(NRF_SAADC_INPUT_AIN0 + PIN_HEEL_FSR - 26),
        .pin_n      = NRF_SAADC_INPUT_DISABLED,
    };
    nrf_drv_saadc_channel_init(0, &heel_cfg);
    nrf_saadc_channel_config_t toe_cfg = heel_cfg;
    toe_cfg.pin_p = (nrf_saadc_input_t)(NRF_SAADC_INPUT_AIN0 + PIN_TOE_FSR - 26);
    nrf_drv_saadc_channel_init(1, &toe_cfg);
}

static void fsr_read(uint16_t *heel, uint16_t *toe)
{
    nrf_saadc_value_t val0, val1;
    nrf_drv_saadc_sample_convert(0, &val0);
    nrf_drv_saadc_sample_convert(1, &val1);
    *heel = (val0 < 0) ? 0 : (uint16_t)val0;
    *toe  = (val1 < 0) ? 0 : (uint16_t)val1;
}

/* ---- Simple FFT (128-point for gait) ---- */
static void fft128(float *re, float *im)
{
    int n = 128;
    for (int s = 2; s <= n; s *= 2) {
        int half = s / 2;
        for (int i = 0; i < n; i += s) {
            for (int k = 0; k < half; k++) {
                float a = -2.0f * 3.14159265f * k / s;
                float wr = cosf(a), wi = sinf(a);
                float tr = wr * re[i+k+half] - wi * im[i+k+half];
                float ti = wr * im[i+k+half] + wi * re[i+k+half];
                re[i+k+half] = re[i+k] - tr;
                im[i+k+half] = im[i+k] - ti;
                re[i+k]      += tr;
                im[i+k]      += ti;
            }
        }
    }
}

/* ---- Gait analysis + FOG detection ---- */
static void analyze_gait(void)
{
    /* FFT on vertical acceleration (az) */
    float re[128], im[128];
    memset(im, 0, sizeof(im));
    for (int i = 0; i < 128; i++) {
        re[i] = g_az[i + GAIT_WINDOW - 128];  /* last 128 samples */
    }
    /* Hann window */
    for (int i = 0; i < 128; i++) {
        float w = 0.5f * (1.0f - cosf(2.0f * 3.14159265f * i / 127));
        re[i] *= w;
    }
    fft128(re, im);

    /* Frequency bands */
    float bw = (float)SAMPLE_RATE_HZ / 128.0f;  /* 0.78 Hz/bin */
    /* Power 0–3 Hz (normal walking stride rate) */
    int k_0_3 = (int)(3.0f / bw);
    /* Power 3–8 Hz (FOG trembling band) */
    int k_3_8 = (int)(8.0f / bw);

    float pwr_lo = 0.0f;  /* 0–3 Hz */
    for (int k = 1; k <= k_0_3; k++) pwr_lo += re[k]*re[k] + im[k]*im[k];
    float pwr_hi = 0.0f;  /* 3–8 Hz */
    for (int k = k_0_3 + 1; k <= k_3_8; k++) pwr_hi += re[k]*re[k] + im[k]*im[k];

    /* Freeze Index (Moore et al., 2007) */
    g_freeze_index = pwr_hi / (pwr_lo + pwr_hi + 0.0001f);

    /* FOG detected if FI > 0.5 for sustained period */
    static int fog_counter = 0;
    if (g_freeze_index > 0.5f) {
        fog_counter++;
        if (fog_counter > 25) {  /* 0.5 s at 50 Hz effective */
            g_fog_detected = true;
            NRF_LOG_WARNING("FOG DETECTED: FI=%.2f", g_freeze_index);
        }
    } else {
        if (fog_counter > 0) fog_counter--;
        if (fog_counter == 0) g_fog_detected = false;
    }

    /* Cadence estimation: dominant frequency in 0.5–3 Hz × 60 */
    float max_pwr = 0.0f;
    int max_k = 1;
    for (int k = 1; k <= k_0_3; k++) {
        float p = re[k]*re[k] + im[k]*im[k];
        if (p > max_pwr) { max_pwr = p; max_k = k; }
    }
    float stride_freq = max_k * bw;  /* Hz */
    g_cadence = stride_freq * 60.0f;  /* steps/min (assuming one stride per cycle) */

    /* Stride length estimate: integrate horizontal acceleration */
    /* Simplified: use peak-to-peak acceleration amplitude */
    float ax_pp = 0.0f;
    float ax_min = 999.0f, ax_max = -999.0f;
    for (int i = 0; i < GAIT_WINDOW; i++) {
        if (g_ax[i] < ax_min) ax_min = g_ax[i];
        if (g_ax[i] > ax_max) ax_max = g_ax[i];
    }
    ax_pp = ax_max - ax_min;
    /* Stride length ∝ sqrt(acceleration amplitude / freq²) */
    g_stride_length = sqrtf(ax_pp / (stride_freq * stride_freq + 0.01f)) * 0.5f;

    /* Festination: stride length decreasing + cadence increasing */
    static float prev_stride = 0.0f, prev_cadence = 0.0f;
    if (prev_stride > 0.0f) {
        if (g_stride_length < prev_stride * 0.9f &&
            g_cadence > prev_cadence * 1.1f) {
            g_festination = true;
            NRF_LOG_WARNING("FESTINATION detected");
        } else {
            g_festination = false;
        }
    }
    prev_stride  = g_stride_length;
    prev_cadence = g_cadence;

    NRF_LOG_INFO("Gait: stride=%.2f m cadence=%.0f FI=%.2f FOG=%d fest=%d",
                 g_stride_length, g_cadence, g_freeze_index,
                 (int)g_fog_detected, (int)g_festination);
}

/* ---- SX1262 radio transmit ---- */
static void sx1262_transmit(const uint8_t *data, size_t len)
{
    /* In production: SX1262 TX via SPI */
    NRF_LOG_INFO("SX1262 TX %d bytes", (int)len);
}

/* ---- Sub-GHz TDMA node task ---- */
static mesh_state_t g_mesh;
static uint16_t g_seq = 0;

static void send_gait_data(void)
{
    gait_payload_t p;
    p.stride_length  = g_stride_length;
    p.cadence        = g_cadence;
    p.freeze_index   = g_freeze_index;
    p.fog_detected   = g_fog_detected ? 1 : 0;
    p.festination    = g_festination ? 1 : 0;
    p.battery_pct    = g_battery_pct;

    mesh_frame_t frame;
    protocol_build_frame(&frame, g_mesh.self_id, NODE_ID_HUB,
                         MSG_TYPE_SENSOR_DATA, g_seq++,
                         (uint8_t *)&p, sizeof(p));
    sx1262_transmit((uint8_t *)&frame, FRAME_MAX_LEN);

    /* If FOG detected, also send FOG alert for haptic cueing */
    if (g_fog_detected) {
        uint8_t cue_payload = HAPTIC_METRONOME_100;
        protocol_build_frame(&frame, g_mesh.self_id, NODE_ID_HUB,
                             MSG_TYPE_FOG_WARNING, g_seq++,
                             &cue_payload, 1);
        sx1262_transmit((uint8_t *)&frame, FRAME_MAX_LEN);
    }
}

/* ---- IMU interrupt ---- */
static void imu_int_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    float ax, ay, az;
    imu_read_sample(&ax, &ay, &az);
    g_ax[g_sample_idx] = ax;
    g_az[g_sample_idx] = az;
    g_sample_idx++;
    if (g_sample_idx >= GAIT_WINDOW) {
        g_sample_idx = 0;
        g_window_ready = true;
    }

    /* FSR read + step detection */
    fsr_read(&g_heel_pressure, &g_toe_pressure);
    if (g_heel_pressure > FSR_THRESHOLD && !g_heel_strike) {
        g_heel_strike = true;
        g_step_count++;
    } else if (g_heel_pressure < FSR_THRESHOLD / 2) {
        g_heel_strike = false;
    }
}

/* ---- Main ---- */
int main(void)
{
    NRF_LOG_INIT(NULL);
    NRF_LOG_INFO("TremorSync Gait Pod starting...");

    nrfx_gpiote_init();
    nrfx_gpiote_in_config_t int_cfg = NRFX_GPIOTE_CONFIG_IN_SETOLOHI(true);
    nrfx_gpiote_in_init(PIN_IMU_INT, &int_cfg, imu_int_handler);
    nrfx_gpiote_in_event_enable(PIN_IMU_INT, true);

    spi_init();
    imu_init();
    saadc_init();
    mesh_init(&g_mesh, NODE_ID_GAIT_POD, false);

    while (1) {
        nrf_pwr_mgmt_run();

        if (g_window_ready) {
            g_window_ready = false;
            analyze_gait();
            send_gait_data();
        }
    }
}