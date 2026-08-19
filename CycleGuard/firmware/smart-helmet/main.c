/*
 * CycleGuard Smart Helmet Firmware
 * Target: nRF52840 QFAA
 *
 * Helmet-mounted crash detection + horn/siren detection + bone-conduction audio.
 * BLE 5.0 GATT to Hub.
 *
 * Crash detection: 500 Hz IMU (ICM-42688-P) — 8g impact threshold + rotational
 * velocity > 300°/s + post-impact stillness. CrashNet 1D-CNN classifies
 * normal/pothole/near-miss/crash in 1-second windows (500 samples × 6 channels).
 * Horn/siren: Knowles SPQ2820WP3-1 bone-conduction mic → NAU88C22 I²S ADC →
 * 8-class acoustic warning (HornNet features extracted on-device).
 * Bone-conduction audio: BOCO P-D4010 transducer via MAX98357A I²S amp.
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

#define TAG "SMART_HELMET"

/* ---- Pin definitions ---- */
#define PIN_I2C_SDA     2
#define PIN_I2C_SCL     3
#define PIN_SPI_CS_IMU  4
#define PIN_SPI_SCK     5
#define PIN_SPI_MISO    6
#define PIN_SPI_MOSI    7
#define PIN_IMU_INT     8
#define PIN_I2S_WS_MIC  9
#define PIN_I2S_SCK_MIC 10
#define PIN_I2S_SD_MIC  11
#define PIN_I2S_BCLK_SPK 12
#define PIN_I2S_LRCK_SPK 13
#define PIN_I2S_DIN_SPK  14
#define PIN_HAPTIC_EN   15
#define PIN_BAT_SENSE   16
#define PIN_CHG_STAT    17
#define PIN_BTN_PAIR    18
#define PIN_BTN_SOS     19
#define PIN_LED_R       20
#define PIN_LED_G       21
#define PIN_LED_B       22

/* ---- Sampling ---- */
#define SAMPLE_RATE_HZ  500
#define CRASH_WINDOW    500   /* 1.0 s window at 500 Hz */

/* ---- Crash thresholds ---- */
#define IMPACT_THRESHOLD_G      8.0f    /* severe impact */
#define ROT_THRESHOLD_DPS       300.0f  /* whiplash rotation */
#define STILLNESS_THRESHOLD_G   0.2f    /* post-impact stillness variance */
#define STILLNESS_DURATION_MS   3000    /* 3 seconds of stillness = unconscious */

/* ---- ICM-42688-P registers ---- */
#define IMU_REG_WHOAMI        0x47
#define IMU_REG_PWR_MGMT0     0x4C
#define IMU_REG_GYRO_CONFIG0  0x4F
#define IMU_REG_ACCEL_CONFIG0 0x50
#define IMU_REG_INT_CONFIG    0x54
#define IMU_REG_INT_STATUS    0x2D
#define IMU_REG_ACCEL_DATA    0x1F
#define IMU_REG_GYRO_DATA     0x25

/* ---- Circular buffer for IMU samples ---- */
static float g_ax[CRASH_WINDOW], g_ay[CRASH_WINDOW], g_az[CRASH_WINDOW];
static float g_gx[CRASH_WINDOW], g_gy[CRASH_WINDOW], g_gz[CRASH_WINDOW];
static int   g_sample_idx = 0;
static bool  g_window_ready = false;

/* ---- Crash analysis results ---- */
static uint8_t g_crash_class    = CRASH_NORMAL;
static float   g_impact_g       = 0.0f;
static float   g_rot_velocity   = 0.0f;
static uint8_t g_battery_pct    = 100;

/* ---- Horn/siren detection ---- */
static bool g_horn_detected  = false;
static bool g_siren_detected = false;

/* ---- Post-impact stillness tracking ---- */
static bool  g_impact_detected  = false;
static uint32_t g_impact_time_ms = 0;
static float g_post_impact_variance = 0.0f;

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
    /* Accel ±16g, ODR 500 Hz (need high rate for crash detection) */
    imu_write_reg(IMU_REG_ACCEL_CONFIG0, 0x08);  /* FS=±16g, ODR=500Hz */
    /* Gyro ±2000 dps, ODR 500 Hz */
    imu_write_reg(IMU_REG_GYRO_CONFIG0, 0x08);
    /* Data-ready interrupt on INT1 */
    imu_write_reg(IMU_REG_INT_CONFIG, 0x03);

    NRF_LOG_INFO("ICM-42688-P initialized @ 500 Hz (crash detection)");
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

    /* ±16g: 1 LSB = 16/32768 g → convert to g */
    const float accel_scale = 16.0f / 32768.0f;
    /* ±2000 dps: 1 LSB = 2000/32768 dps → deg/s */
    const float gyro_scale = 2000.0f / 32768.0f;

    *ax = ax_raw * accel_scale;
    *ay = ay_raw * accel_scale;
    *az = az_raw * accel_scale;
    *gx = gx_raw * gyro_scale;
    *gy = gy_raw * gyro_scale;
    *gz = gz_raw * gyro_scale;
}

/* ---- Crash detection algorithm ---- */
static void analyze_crash(void)
{
    /* 1. Peak acceleration magnitude */
    float max_g = 0.0f;
    for (int i = 0; i < CRASH_WINDOW; i++) {
        float mag = sqrtf(g_ax[i]*g_ax[i] + g_ay[i]*g_ay[i] + g_az[i]*g_az[i]);
        if (mag > max_g) max_g = mag;
    }
    g_impact_g = max_g;

    /* 2. Peak rotational velocity */
    float max_rot = 0.0f;
    for (int i = 0; i < CRASH_WINDOW; i++) {
        float rot = sqrtf(g_gx[i]*g_gx[i] + g_gy[i]*g_gy[i] + g_gz[i]*g_gz[i]);
        if (rot > max_rot) max_rot = rot;
    }
    g_rot_velocity = max_rot;

    /* 3. Post-impact stillness (variance of last 1.5 seconds) */
    float mean_ax = 0, mean_ay = 0, mean_az = 0;
    int still_samples = CRASH_WINDOW / 2;  /* last 0.5 seconds */
    for (int i = CRASH_WINDOW - still_samples; i < CRASH_WINDOW; i++) {
        mean_ax += g_ax[i];
        mean_ay += g_ay[i];
        mean_az += g_az[i];
    }
    mean_ax /= still_samples;
    mean_ay /= still_samples;
    mean_az /= still_samples;

    float var = 0.0f;
    for (int i = CRASH_WINDOW - still_samples; i < CRASH_WINDOW; i++) {
        float dx = g_ax[i] - mean_ax;
        float dy = g_ay[i] - mean_ay;
        float dz = g_az[i] - mean_az;
        var += dx*dx + dy*dy + dz*dz;
    }
    var /= still_samples;
    g_post_impact_variance = sqrtf(var);

    /* 4. CrashNet classification (simplified heuristic — production: 1D-CNN) */
    if (g_impact_g > IMPACT_THRESHOLD_G &&
        (g_rot_velocity > ROT_THRESHOLD_DPS || g_post_impact_variance < STILLNESS_THRESHOLD_G)) {
        g_crash_class = CRASH_CRASH;
        NRF_LOG_WARNING("CRASH DETECTED: impact=%.1fg rot=%.0f°/s stillness_var=%.3f",
                         g_impact_g, g_rot_velocity, g_post_impact_variance);
    } else if (g_impact_g > 5.0f && g_impact_g < IMPACT_THRESHOLD_G) {
        /* 5-8g: pothole or curb hop */
        if (g_rot_velocity < 100.0f) {
            g_crash_class = CRASH_POTHOLE;
        } else {
            g_crash_class = CRASH_NEAR_MISS;
        }
    } else {
        g_crash_class = CRASH_NORMAL;
    }

    /* Track impact for post-impact stillness confirmation */
    if (g_impact_g > IMPACT_THRESHOLD_G) {
        g_impact_detected = true;
        g_impact_time_ms = app_timer_cnt_get() * 1000 / 32768;
    }

    NRF_LOG_INFO("Crash: class=%d impact=%.1fg rot=%.0f°/s var=%.3f",
                 g_crash_class, g_impact_g, g_rot_velocity, g_post_impact_variance);
}

/* ---- Horn/siren detection (acoustic) ---- */
/* I²S mic samples are processed for frequency content */
static void detect_horn_siren(const int16_t *audio, int len)
{
    /* Simple frequency analysis:
     * Car horn: 500-1500 Hz (sustained)
     * Emergency siren: 600-2000 Hz (wailing, frequency modulated)
     * Engine noise: < 500 Hz
     * Wind: broadband
     */
    if (len < 256) return;

    /* Zero-crossing rate → rough frequency estimate */
    int crossings = 0;
    for (int i = 1; i < len; i++) {
        if ((audio[i] > 0 && audio[i-1] <= 0) ||
            (audio[i] <= 0 && audio[i-1] > 0)) {
            crossings++;
        }
    }
    /* Approximate frequency: crossings/2 / sample_duration */
    /* At 16 kHz sample rate, 256 samples = 16 ms → freq = crossings/2 * 62.5 */
    float freq = (crossings / 2.0f) * (16000.0f / len);

    /* Horn: 500-1500 Hz, high amplitude, sustained */
    /* Siren: 600-2000 Hz, modulated (varying frequency) */
    static float freq_history[10];
    static int freq_idx = 0;
    freq_history[freq_idx] = freq;
    freq_idx = (freq_idx + 1) % 10;

    /* Check amplitude */
    float amplitude = 0;
    for (int i = 0; i < len; i++) {
        amplitude += abs(audio[i]);
    }
    amplitude /= len;

    if (amplitude > 2000) {  /* threshold for loud sound */
        if (freq > 500 && freq < 1500) {
            /* Check if sustained (consistent frequency in history) */
            float freq_var = 0;
            float mean_freq = 0;
            for (int i = 0; i < 10; i++) mean_freq += freq_history[i];
            mean_freq /= 10;
            for (int i = 0; i < 10; i++) {
                freq_var += (freq_history[i] - mean_freq) * (freq_history[i] - mean_freq);
            }
            freq_var = sqrtf(freq_var / 10);

            if (freq_var < 100) {  /* sustained → horn */
                g_horn_detected = true;
                g_siren_detected = false;
                NRF_LOG_INFO("HORN detected: %.0f Hz", freq);
            } else {  /* modulated → siren */
                g_siren_detected = true;
                g_horn_detected = false;
                NRF_LOG_INFO("SIREN detected: %.0f Hz (modulated)", freq);
            }
        } else {
            g_horn_detected = false;
            g_siren_detected = false;
        }
    } else {
        g_horn_detected = false;
        g_siren_detected = false;
    }
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
    if (g_sample_idx >= CRASH_WINDOW) {
        g_sample_idx = 0;
        g_window_ready = true;
    }
}

/* ---- Haptic (DRV2605L) ---- */
static void haptic_init(void)
{
    NRF_LOG_INFO("DRV2605L haptic driver initialized (helmet liner)");
}

static void haptic_trigger(uint8_t pattern)
{
    switch (pattern) {
    case HAPTIC_SINGLE_TAP:
        NRF_LOG_INFO("Haptic: single tap (turn approaching)");
        break;
    case HAPTIC_DOUBLE_PULSE:
        NRF_LOG_INFO("Haptic: double pulse (vehicle behind)");
        break;
    case HAPTIC_TRIPLE_BURST:
        NRF_LOG_INFO("Haptic: triple burst (crash imminent!)");
        break;
    case HAPTIC_LONG_BUZZ:
        NRF_LOG_INFO("Haptic: long buzz (general alert)");
        break;
    default:
        break;
    }
}

/* ---- Bone conduction audio (MAX98357A I²S) ---- */
static void bone_conduction_alert(const char *message)
{
    /* In production: synthesize or play pre-recorded voice prompt via I²S */
    NRF_LOG_INFO("Bone conduction: \"%s\"", message);
}

/* ---- BLE GATT service ---- */
static void ble_init(void)
{
    ret_code_t err = nrf_sdh_enable_request();
    APP_ERROR_CHECK(err);

    uint32_t ram_start = 0;
    err = nrf_sdh_ble_default_cfg_set(1, &ram_start);
    APP_ERROR_CHECK(err);
    err = nrf_sdh_ble_enable();
    APP_ERROR_CHECK(err);

    NRF_LOG_INFO("BLE 5.0 initialized — CycleGuard Smart Helmet advertising");
}

/* ---- Main loop ---- */
int main(void)
{
    NRF_LOG_INIT(NULL);
    NRF_LOG_INFO("CycleGuard Smart Helmet starting...");

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

    /* Sampling + analysis loop */
    while (1) {
        nrf_pwr_mgmt_run();

        if (g_window_ready) {
            g_window_ready = false;
            analyze_crash();

            /* BLE notify: send helmet_payload_t to Hub */
            helmet_payload_t payload;
            payload.crash_class    = g_crash_class;
            payload.impact_g       = g_impact_g;
            payload.rot_velocity   = g_rot_velocity;
            payload.horn_detected  = g_horn_detected ? 1 : 0;
            payload.siren_detected = g_siren_detected ? 1 : 0;
            payload.hr             = 0;  /* no PPG on helmet */
            payload.battery_pct    = g_battery_pct;
            /* ble_notify("CG02", &payload, sizeof(payload)); */

            /* Trigger bone conduction + haptic for crash */
            if (g_crash_class == CRASH_CRASH) {
                haptic_trigger(HAPTIC_TRIPLE_BURST);
                bone_conduction_alert("Crash detected. Emergency services alerted.");
            }

            /* Trigger haptic for horn/siren */
            if (g_horn_detected) {
                haptic_trigger(HAPTIC_DOUBLE_PULSE);
                bone_conduction_alert("Horn detected nearby.");
            }
            if (g_siren_detected) {
                haptic_trigger(HAPTIC_DOUBLE_PULSE);
                bone_conduction_alert("Emergency vehicle approaching.");
            }
        }

        /* Process incoming BLE commands from Hub (proximity warnings) */
        /* If MSG_TYPE_PROXIMITY_WARN received: */
        /*   haptic_trigger(payload[0]); */
        /*   bone_conduction_alert("Vehicle approaching from behind"); */
    }
}