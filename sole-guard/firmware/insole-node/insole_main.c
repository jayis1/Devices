/*
 * insole_main.c — SoleGuard Smart Insole node firmware (nRF52840, Zephyr RTOS)
 *
 * Samples 24 FSRs + 8 thermistors + LSM6DSO32 IMU, computes per-step
 * pressure features and per-5-min temperature, transmits to the hub
 * over the BLE mesh every 30s. Low-power: burst-samples during stance.
 *
 * SPDX-License-Identifier: MIT
 */
#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys_clock.h>
#include "sole_protocol.h"

LOG_MODULE_REGISTER(insole, LOG_LEVEL_INF);

#define NODE_ID  SOLE_NODE_ID_INSOLE_L   /* override at build time for right */

/* ---- ADC configuration for 24 FSRs via two 16:1 analog muxes (CD74HC4067) ---- */
#define FSR_COUNT       24
#define TEMP_COUNT       8
#define SAMPLE_RATE_HZ  100
#define REPORT_PERIOD_S 30

/* FSR voltage divider -> ADC 12-bit (0-3.6V). Map to 0-500 kPa (0-255). */
#define ADC_MAX        4095
#define PRESSURE_KPA_MAX 500

static const struct device *adc_dev;
static const struct device *imu_dev;

/* ADC channel for the FSR mux output and thermistor mux output */
static struct adc_channel_cfg fsr_chan_cfg = {
    .gain             = ADC_GAIN_1_6,
    .reference        = ADC_REF_INTERNAL,
    .acquisition_time = ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 40),
    .channel_id       = 0,
    .differential     = 0,
};

static struct adc_sequence fsr_seq = {
    .options    = NULL,
    .channels   = BIT(0),
    .buffer     = NULL,
    .buffer_size = sizeof(uint16_t),
    .resolution = 12,
    .oversampling = 4,
};

/* Mux select pins (4 bits each): MUX_S0..S3 on GPIO0.17..0.20 */
static const struct device *gpio0;
struct gpio_dt_spec mux_sel[4] = {
    GPIO_DT_SPEC_GET(DT_ALIAS(mux_s0), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(mux_s1), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(mux_s2), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(mux_s3), gpios),
};

static uint8_t pressure_raw[FSR_COUNT];        /* 0-255 scaled */
static int16_t temp_centic[TEMP_COUNT];         /* centi-degC */
static int16_t gait[SOLE_GAIT_FEATURES];

/* Running counters for the 30s reporting window */
static uint32_t pti_accum[6];                   /* per-zone PTI accumulator */
static uint16_t step_count_window;
static uint8_t  peak_per_zone[6];

/* ---- Pressure matrix scanning ---- */
static void select_mux_channel(uint8_t ch)
{
    for (int i = 0; i < 4; i++)
        gpio_pin_set_dt(&mux_sel[i], (ch >> i) & 1);
}

static uint16_t adc_sample(uint8_t channel_id)
{
    uint16_t buf = 0;
    fsr_seq.buffer = &buf;
    fsr_seq.channels = BIT(channel_id);
    if (adc_read(adc_dev, &fsr_seq) != 0)
        return 0;
    return buf;
}

static uint8_t scale_pressure(uint16_t adc_val)
{
    /* ADC 12-bit (0-4095) -> pressure 0-255 (0-500 kPa) */
    uint32_t scaled = ((uint32_t)adc_val * 255u) / ADC_MAX;
    return (uint8_t)MIN(scaled, 255u);
}

static void scan_pressure_matrix(void)
{
    for (int i = 0; i < FSR_COUNT; i++) {
        select_mux_channel((uint8_t)i);
        k_busy_wait(60); /* settle 60us */
        uint16_t raw = adc_sample(0);
        pressure_raw[i] = scale_pressure(raw);
        /* accumulate per-zone peak + PTI proxy */
        uint8_t zone = sole_zone_of_sensor((uint8_t)i);
        if (pressure_raw[i] > peak_per_zone[zone])
            peak_per_zone[zone] = pressure_raw[i];
        pti_accum[zone] += pressure_raw[i];
    }
}

/* ---- Thermistor array (8x NTC 10k, via second 16:1 mux on ADC channel 1) ---- */
/* Beta-parameter equation; NTC 10k @25C, B=3977, 10k pull-up to VDD.
   Returns centi-degC. */
static int16_t ntc_to_centic(uint16_t adc_val)
{
    if (adc_val == 0 || adc_val >= ADC_MAX) return 0;
    /* V_div = adc/4095 * Vref; R_ntc = R_pull * V_div / (VDD - V_div)
       For ratiometric ADC with Vref=VDD: r = adc/(ADC_MAX-adc); R = R_pull * r */
    float r = (float)adc_val / (float)(ADC_MAX - adc_val);
    float R = 10000.0f * r;
    const float B = 3977.0f;
    const float R25 = 10000.0f;
    const float T0 = 298.15f;
    /* 1/T = 1/T0 + (1/B)*ln(R/R25) */
    float invT = 1.0f / T0 + (1.0f / B) * logf(R / R25);
    float T = 1.0f / invT - 273.15f;
    return (int16_t)(T * 100.0f);
}

static void scan_thermistors(void)
{
    for (int i = 0; i < TEMP_COUNT; i++) {
        select_mux_channel((uint8_t)(i + 8)); /* thermistors on mux channels 8-15 */
        k_busy_wait(200); /* NTC settle 200us */
        uint16_t raw = adc_sample(1);
        temp_centic[i] = ntc_to_centic(raw);
    }
}

/* ---- IMU / gait phase (LSM6DSO32) ---- */
static void sample_gait(void)
{
    struct sensor_value accel[3], gyro[3];
    if (sensor_sample_fetch(imu_dev) != 0) return;
    sensor_channel_get(imu_dev, SENSOR_CHAN_ACCEL_XYZ, accel);
    sensor_channel_get(imu_dev, SENSOR_CHAN_GYRO_XYZ,  gyro);

    /* Vertical accel magnitude for heel-strike / toe-off detection */
    float az = sensor_value_to_double(&accel[2]);
    /* Simple step detection: threshold on |az| > 1.5g at heel-strike */
    static int in_stance;
    if (fabsf(az) > 1.5f * 9.81f && !in_stance) {
        in_stance = 1;
        step_count_window++;
    } else if (fabsf(az) < 1.0f * 9.81f) {
        in_stance = 0;
    }

    /* Fill gait feature slots (simplified; full pipeline in gait_phase.c) */
    gait[6] = (int16_t)step_count_window; /* step count this window */
    /* cadence proxy: steps / 0.5 min * 2 = spm */
    gait[0] = (int16_t)(step_count_window * 4); /* approx spm over 30s */
}

/* ---- Mesh publish ---- */
static void publish_pressure_temp(uint8_t seq)
{
    sole_pressure_payload_t p = {0};
    p.type     = SOLE_MSG_PRESSURE_TEMP;
    p.node_id  = NODE_ID;
    p.seq      = seq;
    p.flags    = 0;
    memcpy(p.pressure, pressure_raw, FSR_COUNT);
    memcpy(p.temp_centic, temp_centic, sizeof(temp_centic));
    /* worst-zone PTI proxy in centi-kPa*s */
    uint32_t max_pti = 0;
    for (int z = 0; z < 6; z++)
        if (pti_accum[z] > max_pti) max_pti = pti_accum[z];
    p.pti_centic = (uint16_t)MIN(max_pti / 100u, 65535u);
    sole_pack_crc(&p, sizeof(p) - 2);

    extern int sole_mesh_publish_model0(const void *data, size_t len);
    sole_mesh_publish_model0(&p, sizeof(p));
}

static void publish_gait(uint8_t seq)
{
    sole_gait_payload_t p = {0};
    p.type    = SOLE_MSG_GAIT;
    p.node_id = NODE_ID;
    p.seq     = seq;
    p.flags   = 0;
    memcpy(p.gait, gait, sizeof(gait));
    sole_pack_crc(&p, sizeof(p) - 2);

    extern int sole_mesh_publish_model0(const void *data, size_t len);
    sole_mesh_publish_model0(&p, sizeof(p));
}

/* ---- Main loop ---- */
int main(void)
{
    adc_dev = DEVICE_DT_GET(DT_NODELABEL(adc));
    imu_dev = DEVICE_DT_GET(DT_NODELABEL(lsm6dso32));
    gpio0   = DEVICE_DT_GET(DT_NODELABEL(gpio0));

    if (!device_is_ready(adc_dev) || !device_is_ready(imu_dev)) {
        LOG_ERR("Devices not ready");
        return -1;
    }
    adc_channel_setup(adc_dev, &fsr_chan_cfg);
    for (int i = 0; i < 4; i++)
        gpio_pin_configure_dt(&mux_sel[i], GPIO_OUTPUT);

    LOG_INF("SoleGuard insole node %d starting", NODE_ID);

    uint8_t seq = 0;
    int64_t last_report = k_uptime_get();
    int64_t last_temp   = k_uptime_get();

    while (1) {
        /* Burst-sample pressure at 100 Hz for 100ms windows, then sleep */
        for (int s = 0; s < 10; s++) {
            scan_pressure_matrix();
            sample_gait();
            k_msleep(10); /* ~100 Hz effective during active window */
        }
        /* Temperature is slow — sample every 10s */
        if (k_uptime_get() - last_temp > 10000) {
            scan_thermistors();
            last_temp = k_uptime_get();
        }
        /* Report every 30s */
        if (k_uptime_get() - last_report > REPORT_PERIOD_S * 1000) {
            publish_pressure_temp(seq);
            publish_gait(seq);
            /* reset window counters */
            memset(pti_accum, 0, sizeof(pti_accum));
            memset(peak_per_zone, 0, sizeof(peak_per_zone));
            step_count_window = 0;
            seq++;
            last_report = k_uptime_get();
        }
        /* Deep sleep 500ms between burst windows when inactive */
        k_msleep(500);
    }
    return 0;
}