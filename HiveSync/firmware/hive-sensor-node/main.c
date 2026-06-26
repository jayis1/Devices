/*
 * HiveSync — Hive Sensor Node Firmware
 * MCU: STM32L476RG
 * Sensors: SHT45×3, HX711, LIS3DH, ICS-43434
 * Radio: CC1101 (868 MHz Sub-GHz)
 */

#include <string.h>
#include <math.h>
#include "stm32l4xx_hal.h"
#include "cc1101.h"
#include "sht45.h"
#include "hx711.h"
#include "lis3dh.h"
#include "ics43434.h"
#include "hivesync_proto.h"

/* ---- Constants ---- */
#define NODE_TYPE       NODE_SENSOR
#define FW_VERSION      0x0100
#define SAMPLE_INTERVAL_S   300   /* 5 min default */
#define AUDIO_FFT_SIZE      256
#define WEIGHT_STABLE_COUNT 5
#define TEMP_COUNT           3

/* ---- Global State ---- */
typedef struct {
    uint16_t node_id;
    uint32_t sample_interval_s;
    float temp_c[TEMP_COUNT];        /* brood, top, entrance */
    float humidity_pct;
    float weight_kg;
    float weight_delta_g;
    float accel_rms_mg;
    float battery_mv;
    int8_t  rssi_dbm;
    /* Audio features */
    float spectral_centroid_hz;
    float spectral_bandwidth_hz;
    float peak_freq_hz;
    float peak_amplitude_db;
    /* Flags */
    uint8_t swarm_alert;
    uint8_t disturbance_alert;
} sensor_data_t;

static sensor_data_t g_data;
static volatile uint8_t g_wakeup_flag = 0;
static uint8_t g_tx_buffer[128];

/* ---- Peripherals (handles set in main) ---- */
extern I2C_HandleTypeDef hi2c1;
extern SPI_HandleTypeDef hspi1;
extern UART_HandleTypeDef huart2;
extern ADC_HandleTypeDef hadc1;

static cc1101_t g_radio;
static sht45_t  g_sht[TEMP_COUNT];
static hx711_t  g_load;
static lis3dh_t g_accel;
static ics43434_t g_mic;

/* ---- Low-Power Timer Callback ---- */
void HAL_LPTIM_AutoReloadMatchCallback(LPTIM_HandleTypeDef *hlptim) {
    g_wakeup_flag = 1;
}

/* ---- FFT (Cooley-Tukey, radix-2, in-place) ---- */
static void fft_radix2(float *real, float *imag, int n) {
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        while (j & bit) { j ^= bit; bit >>= 1; }
        j ^= bit;
        if (i < j) {
            float tmp;
            tmp = real[i]; real[i] = real[j]; real[j] = tmp;
            tmp = imag[i]; imag[i] = imag[j]; imag[j] = tmp;
        }
    }
    for (int len = 2; len <= n; len <<= 1) {
        float ang = -2.0f * M_PI / len;
        float wr = cosf(ang), wi = sinf(ang);
        for (int i = 0; i < n; i += len) {
            float cr = 1.0f, ci = 0.0f;
            for (int j = 0; j < len / 2; j++) {
                float tr = cr * real[i+j+len/2] - ci * imag[i+j+len/2];
                float ti = cr * imag[i+j+len/2] + ci * real[i+j+len/2];
                real[i+j+len/2] = real[i+j] - tr;
                imag[i+j+len/2] = imag[i+j] - ti;
                real[i+j] += tr;
                imag[i+j] += ti;
                float nw = cr * wr - ci * wi;
                ci = cr * wi + ci * wr;
                cr = nw;
            }
        }
    }
}

/* ---- Extract Audio Features ---- */
static void extract_audio_features(float *real, float *imag, int n, float sample_rate) {
    fft_radix2(real, imag, n);
    /* Spectral centroid */
    float sum_mag = 0, sum_weighted = 0;
    float max_mag = 0;
    int max_idx = 0;
    float bw_sum = 0;
    for (int i = 0; i < n / 2; i++) {
        float mag = sqrtf(real[i]*real[i] + imag[i]*imag[i]);
        float freq = (float)i * sample_rate / n;
        sum_mag += mag;
        sum_weighted += freq * mag;
        if (mag > max_mag) { max_mag = mag; max_idx = i; }
    }
    g_data.spectral_centroid_hz = (sum_mag > 0) ? sum_weighted / sum_mag : 0;
    g_data.peak_freq_hz = (float)max_idx * sample_rate / n;
    g_data.peak_amplitude_db = 20.0f * log10f(max_mag / (n/2) + 1e-10f);
    /* Spectral bandwidth */
    float centroid = g_data.spectral_centroid_hz;
    for (int i = 0; i < n / 2; i++) {
        float mag = sqrtf(real[i]*real[i] + imag[i]*imag[i]);
        float freq = (float)i * sample_rate / n;
        bw_sum += mag * (freq - centroid) * (freq - centroid);
    }
    g_data.spectral_bandwidth_hz = (sum_mag > 0) ? sqrtf(bw_sum / sum_mag) : 0;
}

/* ---- Sample Sensors ---- */
static void sample_sensors(void) {
    /* Temperature & Humidity (3 SHT45 sensors) */
    for (int i = 0; i < TEMP_COUNT; i++) {
        sht45_read(&g_sht[i], &g_data.temp_c[i], NULL);
    }
    sht45_read(&g_sht[0], NULL, &g_data.humidity_pct); /* Use brood sensor for humidity */

    /* Weight (HX711) */
    float new_weight = hx711_read_kg(&g_load);
    if (new_weight > 0) {
        g_data.weight_delta_g = (new_weight - g_data.weight_kg) * 1000.0f;
        g_data.weight_kg = new_weight;
    }

    /* Accelerometer (LIS3DH) — RMS over 100 samples */
    float ax, ay, az;
    float sum_sq = 0;
    for (int i = 0; i < 100; i++) {
        lis3dh_read_accel(&g_accel, &ax, &ay, &az);
        sum_sq += ax*ax + ay*ay + az*az;
    }
    g_data.accel_rms_mg = sqrtf(sum_sq / 100.0f) * 1000.0f;

    /* Microphone — capture 256 samples at 16 kHz, FFT, extract features */
    float real_buf[AUDIO_FFT_SIZE], imag_buf[AUDIO_FFT_SIZE];
    ics43434_start_capture(&g_mic, AUDIO_FFT_SIZE, 16000);
    ics43434_read_samples(&g_mic, real_buf, AUDIO_FFT_SIZE);
    memset(imag_buf, 0, sizeof(imag_buf));
    extract_audio_features(real_buf, imag_buf, AUDIO_FFT_SIZE, 16000.0f);

    /* Battery voltage (ADC on internal VREF) */
    g_data.battery_mv = (float)adc_read_vbat_mv(&hadc1);
}

/* ---- Pack & Transmit ---- */
static void pack_sensor_data(uint8_t *buf, uint16_t *len) {
    hivesync_header_t hdr = {
        .msg_type = MSG_DATA,
        .src_id = g_data.node_id,
        .dst_id = 0x0000, /* gateway */
        .version = FW_VERSION,
    };
    *len = hivesync_pack_data(buf, &hdr,
        g_data.temp_c, TEMP_COUNT,
        g_data.humidity_pct,
        g_data.weight_kg,
        g_data.weight_delta_g,
        g_data.accel_rms_mg,
        g_data.battery_mv,
        g_data.spectral_centroid_hz,
        g_data.peak_freq_hz,
        g_data.peak_amplitude_db,
        g_data.spectral_bandwidth_hz
    );
}

static void transmit_data(void) {
    uint16_t len;
    pack_sensor_data(g_tx_buffer, &len);
    cc1101_tx_packet(&g_radio, g_tx_buffer, len);
    g_data.rssi_dbm = cc1101_last_rssi(&g_radio);
}

/* ---- Initialize Hardware ---- */
static void init_hardware(void) {
    /* SHT45 sensors at addresses 0x44, 0x45, 0x46 */
    sht45_init(&g_sht[0], &hi2c1, SHT45_ADDR_A);
    sht45_init(&g_sht[1], &hi2c1, SHT45_ADDR_B);
    sht45_init(&g_sht[2], &hi2c1, SHT45_ADDR_C);

    /* HX711 load cell */
    hx711_init(&g_load, HX711_SCK_PIN, HX711_DOUT_PIN, 2280.0f); /* calibration factor */

    /* LIS3DH accelerometer */
    lis3dh_init(&g_accel, &hi2c1, LIS3DH_ADDR);

    /* ICS-43434 microphone */
    ics43434_init(&g_mic, &hi2c1);

    /* CC1101 Sub-GHz radio */
    cc1101_init(&g_radio, &hspi1, CC1101_CS_PIN, CC1101_IRQ_PIN);
    cc1101_set_frequency(&g_radio, 868000000);
    cc1101_set_power(&g_radio, CC1101_POWER_12DBM);
    cc1101_set_channel(&g_radio, 0); /* TDMA slot assigned by gateway */
}

/* ---- Main Loop ---- */
int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_SPI1_Init();
    MX_USART2_Init();
    MX_ADC1_Init();
    MX_LPTIM1_Init();

    init_hardware();

    /* Load node ID from OTP / flash */
    g_data.node_id = *(volatile uint16_t*)0x1FFF7A10;
    g_data.sample_interval_s = SAMPLE_INTERVAL_S;

    /* Set LPTIM for periodic wakeup */
    HAL_LPTIM_SetAutoReload(&hlptim1, g_data.sample_interval_s * LSE_FREQ);

    while (1) {
        /* Enter stop mode — woken by LPTIM */
        HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
        SystemClock_Config(); /* Re-init after STOP */

        if (g_wakeup_flag) {
            g_wakeup_flag = 0;
            sample_sensors();
            transmit_data();
        }
    }
}