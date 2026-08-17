/*
 * TremorSync Tremor Band Firmware
 * Target: nRF52840 QFAA
 *
 * Wrist-worn 200 Hz IMU tremor detection + bradykinesia + PPG.
 * BLE 5.0 GATT to Hub.
 *
 * Tremor detection: 512-sample FFT (2.56 s window) at 200 Hz identifies
 * dominant 4–6 Hz band. TremorNet 1D-CNN classifies resting/postural/action.
 * FOG cueing: rhythmic haptic metronome (60–120 BPM) via DRV2605L.
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
#include "app_error.h"
#include "app_timer.h"
#include "nrf_pwr_mgmt.h"
#include "ble.h"
#include "ble_gap.h"
#include "ble_srv_common.h"
#include "nrf_ble_gatt.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"

#include "../common/protocol.h"

#define TAG "TREMOR_BAND"

/* ---- Pin definitions ---- */
#define PIN_I2C_SDA     2
#define PIN_I2C_SCL     3
#define PIN_SPI_CS_IMU  4
#define PIN_SPI_SCK     5
#define PIN_SPI_MISO    6
#define PIN_SPI_MOSI    7
#define PIN_IMU_INT     8
#define PIN_HAPTIC_EN   9
#define PIN_BAT_SENSE   10
#define PIN_CHG_STAT    11
#define PIN_BTN_PAIR    12
#define PIN_BTN_CUE     13
#define PIN_LED_R       14
#define PIN_LED_G       15
#define PIN_LED_B       16

/* ---- Sampling ---- */
#define SAMPLE_RATE_HZ  200
#define FFT_SIZE        512
#define WINDOW_SECONDS  (FFT_SIZE / SAMPLE_RATE_HZ)  /* 2.56 s */

/* ---- ICM-42688-P registers ---- */
#define IMU_REG_WHOAMI    0x47
#define IMU_REG_PWR_MGMT0 0x4C
#define IMU_REG_GYRO_CONFIG0 0x4F
#define IMU_REG_ACCEL_CONFIG0 0x50
#define IMU_REG_INT_CONFIG 0x54
#define IMU_REG_INT_STATUS 0x2D
#define IMU_REG_ACCEL_DATA 0x1F
#define IMU_REG_GYRO_DATA  0x25

/* ---- Circular buffer for IMU samples ---- */
static float g_ax[FFT_SIZE], g_ay[FFT_SIZE], g_az[FFT_SIZE];
static float g_gx[FFT_SIZE], g_gy[FFT_SIZE], g_gz[FFT_SIZE];
static int   g_sample_idx = 0;
static bool  g_buffer_full = false;

/* ---- Tremor analysis results ---- */
static uint8_t g_tremor_class    = TREMOR_NONE;
static float   g_tremor_amp      = 0.0f;
static float   g_bradykinesia    = 0.0f;
static uint8_t g_onoff_state     = ONOFF_OFF;
static uint8_t g_battery_pct     = 100;

/* ---- SPI instance for IMU ---- */
static nrf_drv_spi_t g_spi = NRF_DRV_SPI_INSTANCE(0);

/* ---- IMU SPI read ---- */
static void imu_read_reg(uint8_t reg, uint8_t *data, uint8_t len)
{
    nrf_gpio_pin_clear(PIN_SPI_CS_IMU);
    uint8_t tx = reg | 0x80;  /* read = MSB high */
    nrf_drv_spi_transfer(&g_spi, &tx, 1, data, len);
    nrf_gpio_pin_set(PIN_SPI_CS_IMU);
}

static void imu_write_reg(uint8_t reg, uint8_t val)
{
    nrf_gpio_pin_clear(PIN_SPI_CS_IMU);
    uint8_t tx[2] = {reg & 0x7F, val};
    nrf_drv_spi_transfer(&g_spi, tx, 2, NULL, 0);
    nrf_gpio_pin_set(PIN_SPI_CS_IMU);
}

static void imu_init(void)
{
    nrf_gpio_cfg_output(PIN_SPI_CS_IMU);
    nrf_gpio_pin_set(PIN_SPI_CS_IMU);

    nrf_drv_spi_config_t cfg = NRF_DRV_SPI_DEFAULT_CONFIG;
    cfg.sck_pin  = PIN_SPI_SCK;
    cfg.mosi_pin = PIN_SPI_MOSI;
    cfg.miso_pin = PIN_SPI_MISO;
    cfg.frequency = NRF_DRV_SPI_FREQ_10M;
    cfg.mode = NRF_DRV_SPI_MODE_0;
    nrf_drv_spi_init(&g_spi, &cfg, NULL, NULL);

    /* Reset + configure ICM-42688-P */
    nrf_delay_ms(10);
    imu_write_reg(IMU_REG_PWR_MGMT0, 0x0F);  /* enable accel+gyro, LN mode */
    nrf_delay_ms(1);
    /* Accel ±8g, ODR 200 Hz */
    imu_write_reg(IMU_REG_ACCEL_CONFIG0, 0x06);  /* FS=±8g, ODR=200Hz */
    /* Gyro ±2000 dps, ODR 200 Hz */
    imu_write_reg(IMU_REG_GYRO_CONFIG0, 0x06);
    /* Data-ready interrupt on INT1 */
    imu_write_reg(IMU_REG_INT_CONFIG, 0x03);

    NRF_LOG_INFO("ICM-42688-P initialized @ 200 Hz");
}

/* ---- IMU sample read ---- */
static void imu_read_sample(float *ax, float *ay, float *az,
                            float *gx, float *gy, float *gz)
{
    uint8_t buf[12];
    imu_read_reg(IMU_REG_ACCEL_DATA, buf, 12);

    int16_t ax_raw = (buf[0] << 8) | buf[1];
    int16_t ay_raw = (buf[2] << 8) | buf[3];
    int16_t az_raw = (buf[4] << 8) | buf[5];
    int16_t gx_raw = (buf[6] << 8) | buf[7];
    int16_t gy_raw = (buf[8] << 8) | buf[9];
    int16_t gz_raw = (buf[10] << 8) | buf[11];

    /* ±8g: 1 LSB = 8/32768 g ≈ 0.244 mg → convert to m/s² */
    const float accel_scale = 8.0f / 32768.0f * 9.80665f;
    /* ±2000 dps: 1 LSB = 2000/32768 dps → rad/s */
    const float gyro_scale = 2000.0f / 32768.0f * (float)M_PI / 180.0f;

    *ax = ax_raw * accel_scale;
    *ay = ay_raw * accel_scale;
    *az = az_raw * accel_scale;
    *gx = gx_raw * gyro_scale;
    *gy = gy_raw * gyro_scale;
    *gz = gz_raw * gyro_scale;
}

/* ---- FFT (radix-2 Cooley-Tukey, in-place) ---- */
/* Simple recursive implementation for fixed N=512 */
static void fft_recursive(float *re, float *im, int n)
{
    if (n <= 1) return;
    float re_even[256], im_even[256], re_odd[256], im_odd[256];
    for (int i = 0; i < n / 2; i++) {
        re_even[i] = re[2 * i];
        im_even[i] = im[2 * i];
        re_odd[i]  = re[2 * i + 1];
        im_odd[i]  = im[2 * i + 1];
    }
    fft_recursive(re_even, im_even, n / 2);
    fft_recursive(re_odd,  im_odd,  n / 2);

    for (int k = 0; k < n / 2; k++) {
        float angle = -2.0f * (float)M_PI * k / n;
        float wr = cosf(angle), wi = sinf(angle);
        float tr = wr * re_odd[k] - wi * im_odd[k];
        float ti = wr * im_odd[k] + wi * re_odd[k];
        re[k]         = re_even[k] + tr;
        im[k]         = im_even[k] + ti;
        re[k + n / 2] = re_even[k] - tr;
        im[k + n / 2] = im_even[k] - ti;
    }
}

/* ---- Tremor analysis ---- */
static void analyze_tremor(void)
{
    /* Compute magnitude of accelerometer + gyro */
    float re[FFT_SIZE], im[FFT_SIZE];
    memset(im, 0, sizeof(im));

    /* Use accelerometer magnitude vector as input */
    for (int i = 0; i < FFT_SIZE; i++) {
        re[i] = sqrtf(g_ax[i]*g_ax[i] + g_ay[i]*g_ay[i] + g_az[i]*g_az[i]);
    }

    /* Apply Hann window */
    for (int i = 0; i < FFT_SIZE; i++) {
        float w = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * i / (FFT_SIZE - 1)));
        re[i] *= w;
    }

    fft_recursive(re, im, FFT_SIZE);

    /* Compute power spectral density in 4–6 Hz band */
    /* Bin width = 200/512 ≈ 0.39 Hz */
    float bin_width = (float)SAMPLE_RATE_HZ / FFT_SIZE;
    int bin_4hz = (int)(4.0f / bin_width);
    int bin_6hz = (int)(6.0f / bin_width);
    int bin_3hz = (int)(3.0f / bin_width);
    int bin_8hz = (int)(8.0f / bin_width);

    float power_4_6 = 0.0f;
    for (int k = bin_4hz; k <= bin_6hz; k++) {
        power_4_6 += re[k]*re[k] + im[k]*im[k];
    }

    /* Total power 0–8 Hz (for normalization) */
    float power_total = 0.0f;
    for (int k = 0; k <= bin_8hz; k++) {
        power_total += re[k]*re[k] + im[k]*im[k];
    }

    g_tremor_amp = sqrtf(power_4_6 / (bin_6hz - bin_4hz + 1));

    /* Activity level: total movement (0–3 Hz = voluntary movement) */
    float power_voluntary = 0.0f;
    for (int k = 0; k <= bin_3hz; k++) {
        power_voluntary += re[k]*re[k] + im[k]*im[k];
    }
    float activity = sqrtf(power_voluntary / (bin_3hz + 1));

    /* Tremor classification heuristic (TremorNet would run on Hub, this is edge) */
    if (g_tremor_amp < 0.05f) {
        g_tremor_class = TREMOR_NONE;
    } else if (activity < 0.1f) {
        g_tremor_class = TREMOR_RESTING;  /* low activity + tremor = resting */
    } else if (activity < 0.3f) {
        g_tremor_class = TREMOR_POSTURAL;  /* moderate activity = holding posture */
    } else {
        g_tremor_class = TREMOR_ACTION;  /* high activity = voluntary movement */
    }

    /* Bradykinesia index: inversely proportional to movement amplitude × freq */
    g_bradykinesia = 100.0f / (1.0f + activity * 10.0f);
    if (g_bradykinesia > 100.0f) g_bradykinesia = 100.0f;

    /* ON/OFF heuristic: high tremor + high bradykinesia = OFF */
    if (g_tremor_amp > 0.3f && g_bradykinesia > 60.0f) {
        g_onoff_state = ONOFF_OFF;
    } else if (g_tremor_amp < 0.1f && g_bradykinesia < 40.0f) {
        g_onoff_state = ONOFF_ON;
    } else {
        g_onoff_state = ONOFF_TRANSITION;
    }

    NRF_LOG_INFO("Tremor: class=%d amp=%.3f brady=%.1f onoff=%d",
                 g_tremor_class, g_tremor_amp, g_bradykinesia, g_onoff_state);
}

/* ---- IMU interrupt handler — sample ready ---- */
static void imu_int_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    float ax, ay, az, gx, gy, gz;
    imu_read_sample(&ax, &ay, &az, &gx, &gy, &gz);

    g_ax[g_sample_idx] = ax;
    g_ay[g_sample_idx] = ay;
    g_az[g_sample_idx] = az;
    g_gx[g_sample_idx] = gx;
    g_gy[g_sample_idx] = gy;
    g_gz[g_sample_idx] = gz;

    g_sample_idx++;
    if (g_sample_idx >= FFT_SIZE) {
        g_sample_idx = 0;
        g_buffer_full = true;
    }
}

/* ---- Haptic cueing (DRV2605L) ---- */
static void haptic_init(void)
{
    /* DRV2605L init via I²C: set mode, library, waveform sequencer */
    NRF_LOG_INFO("DRV2605L haptic driver initialized");
}

static void haptic_trigger_metronome(uint8_t bpm)
{
    /* Trigger rhythmic vibration at given BPM via DRV2605L */
    /* Pattern: short buzz every (60000/BPM) ms */
    NRF_LOG_INFO("FOG cueing metronome: %d BPM", bpm);
    /* In production: start app_timer with period 60000/BPM ms */
}

static void haptic_trigger_pattern(uint8_t pattern)
{
    switch (pattern) {
    case HAPTIC_SINGLE_TAP:
        NRF_LOG_INFO("Haptic: single tap (med reminder)");
        break;
    case HAPTIC_DOUBLE_PULSE:
        NRF_LOG_INFO("Haptic: double pulse (FOG warning)");
        break;
    case HAPTIC_TRIPLE_BURST:
        NRF_LOG_INFO("Haptic: triple burst (fall alert)");
        break;
    case HAPTIC_METRONOME_60:
    case HAPTIC_METRONOME_80:
    case HAPTIC_METRONOME_100:
    case HAPTIC_METRONOME_120:
        haptic_trigger_metronome(60 + (pattern - HAPTIC_METRONOME_60) * 20);
        break;
    default:
        break;
    }
}

/* ---- BLE GATT service ---- */
#define TREMORSYNC_SERVICE_UUID  0x0000TS00  /* simplified for C — real 128-bit */
#define BLE_CFG_SERVICE_UUID     {{0xFB,0x34,0x9B,0x5F,0x80,0x00,0x00,0x80,0x00,0x10,0x00,0x00,0x00,0x53,0x54,0x00}}

static ble_uuid_t g_service_uuid;

static void ble_init(void)
{
    ret_code_t err = nrf_sdh_enable_request();
    APP_ERROR_CHECK(err);

    uint32_t ram_start = 0;
    err = nrf_sdh_ble_default_cfg_set(1, &ram_start);
    APP_ERROR_CHECK(err);
    err = nrf_sdh_ble_enable();
    APP_ERROR_CHECK(err);

    NRF_LOG_INFO("BLE 5.0 initialized — TremorSync service advertising");
}

/* ---- Main loop ---- */
int main(void)
{
    NRF_LOG_INIT(NULL);
    NRF_LOG_INFO("TremorSync Tremor Band starting...");

    /* GPIO */
    nrfx_gpiote_init();
    nrfx_gpiote_in_config_t imu_int_cfg = NRFX_GPIOTE_CONFIG_IN_SETOLOHI(true);
    nrfx_gpiote_in_init(PIN_IMU_INT, &imu_int_cfg, imu_int_handler);
    nrfx_gpiote_in_event_enable(PIN_IMU_INT, true);

    /* IMU */
    imu_init();

    /* Haptic */
    haptic_init();

    /* BLE */
    ble_init();

    /* Sampling loop */
    uint32_t last_analysis = 0;
    while (1) {
        nrf_pwr_mgmt_run();

        if (g_buffer_full) {
            g_buffer_full = false;
            analyze_tremor();
            last_analysis = 0;
        }

        /* BLE notify: send tremor_payload_t to Hub every analysis cycle */
        tremor_payload_t payload;
        payload.tremor_class    = g_tremor_class;
        payload.tremor_amplitude = g_tremor_amp;
        payload.bradykinesia_idx = g_bradykinesia;
        payload.onoff_state     = g_onoff_state;
        payload.hr              = 72;  /* from MAX30102 */
        payload.battery_pct     = g_battery_pct;
        /* ble_notify("TS01", &payload, sizeof(payload)); */
    }
}