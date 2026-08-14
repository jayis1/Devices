/*
 * PostureSync Spine Band Firmware
 * Target: nRF52840 QFAA
 *
 * Wearable band worn between shoulder blades.
 * 9-DoF IMU (ICM-42688-P) spine angle tracking via Madgwick AHRS.
 * PPG (MAX30102) heart rate + HRV for stress correlation.
 * BMP390 barometric pressure for altitude/floor tracking.
 * DRV2605L haptic driver for posture correction feedback.
 * BLE 5.0 to Hub.
 *
 * Battery: 402030 LiPo 400mAh, 5-day life.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "nrf.h"
#include "nrf_drv_spi.h"
#include "nrf_drv_twi.h"
#include "nrf_drv_gpiote.h"
#include "nrf_drv_saadc.h"
#include "nrf_delay.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "app_timer.h"
#include "app_error.h"
#include "ble.h"
#include "ble_srv_common.h"
#include "ble_advdata.h"
#include "ble_conn_params.h"
#include "ble_db_discovery.h"
#include "nordic_common.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"

#include "../common/protocol.h"

/* ---- Pin definitions ---- */
#define PIN_I2C_SDA     NRF_GPIO_PIN_MAP(0, 2)
#define PIN_I2C_SCL     NRF_GPIO_PIN_MAP(0, 3)
#define PIN_SPI_CS      NRF_GPIO_PIN_MAP(0, 4)
#define PIN_SPI_SCK     NRF_GPIO_PIN_MAP(0, 5)
#define PIN_SPI_MISO    NRF_GPIO_PIN_MAP(0, 6)
#define PIN_SPI_MOSI    NRF_GPIO_PIN_MAP(0, 7)
#define PIN_IMU_INT     NRF_GPIO_PIN_MAP(0, 8)
#define PIN_HAPTIC_EN   NRF_GPIO_PIN_MAP(0, 9)
#define PIN_BAT_SENSE   NRF_GPIO_PIN_MAP(0, 10)
#define PIN_CHG_STAT    NRF_GPIO_PIN_MAP(0, 11)
#define PIN_BTN_PAIR    NRF_GPIO_PIN_MAP(0, 12)
#define PIN_LED_R       NRF_GPIO_PIN_MAP(0, 13)
#define PIN_LED_G       NRF_GPIO_PIN_MAP(0, 14)
#define PIN_LED_B       NRF_GPIO_PIN_MAP(0, 15)

/* SPI instance for IMU */
#define SPI_INSTANCE    0
static nrf_drv_spi_t spi = NRF_DRV_SPI_INSTANCE(SPI_INSTANCE);
static volatile bool spi_xfer_done = true;

/* I2C instance for PPG, barometer, haptic, fuel gauge */
#define TWI_INSTANCE    0
static nrf_drv_twi_t twi = NRF_DRV_TWI_INSTANCE(TWI_INSTANCE);

/* ICM-42688-P registers */
#define ICM_REG_PWR_MGMT0   0x4E
#define ICM_REG_GYRO_CONFIG0 0x4F
#define ICM_REG_ACCEL_CONFIG0 0x50
#define ICM_REG_ACCEL_DATA   0x0B
#define ICM_REG_GYRO_DATA    0x11
#define ICM_REG_INT_STATUS   0x2D

/* Sampling */
#define SAMPLE_RATE_HZ   200
#define SAMPLE_PERIOD_MS 5

/* BLE */
#define POSTSYNC_SERVICE_UUID  0x5053  /* "PS" */
#define BLE_CONN_CFG_TAG       1
#define APP_TIMER_PRESCALER    0

static uint16_t g_conn_handle = BLE_CONN_HANDLE_INVALID;
static ble_gatts_t g_gatts;
static bool g_ble_connected = false;

/* Madgwick AHRS state */
typedef struct {
    float q0, q1, q2, q3;  /* quaternion */
    float beta;            /* algorithm gain */
    float sample_dt;       /* sample period (s) */
} madgwick_state_t;

static madgwick_state_t g_ahrs = {
    .q0 = 1.0f, .q1 = 0.0f, .q2 = 0.0f, .q3 = 0.0f,
    .beta = 0.1f, .sample_dt = 1.0f / SAMPLE_RATE_HZ
};

/* Calibration */
typedef struct {
    float pitch_offset;
    float roll_offset;
    float yaw_offset;
    float pitch_forward_limit;
    float pitch_backward_limit;
    float roll_left_limit;
    float roll_right_limit;
    bool calibrated;
} calibration_t;

static calibration_t g_cal = {0};

/* Sensor data */
typedef struct {
    float ax, ay, az;  /* accel (g) */
    float gx, gy, gz;  /* gyro (dps) */
} imu_sample_t;

static imu_sample_t g_imu;
static uint8_t g_hr = 0;
static uint8_t g_spo2 = 0;
static uint8_t g_battery = 100;
static uint8_t g_posture_class = POSTURE_NEUTRAL;
static uint8_t g_activity = 0;

/* Timer for periodic sampling */
APP_TIMER_DEF(g_sample_timer);
APP_TIMER_DEF(g_ble_tx_timer);

/* ---- SPI IMU ---- */
static void spi_event_handler(nrf_drv_spi_evt_t const *p_event, void *p_context)
{
    spi_xfer_done = true;
}

static void imu_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t tx[2] = {reg & 0x7F, val};  /* write = MSB=0 */
    nrf_drv_spi_transfer(&spi, tx, 2, NULL, 0);
}

static uint8_t imu_read_reg(uint8_t reg)
{
    uint8_t tx[2] = {reg | 0x80, 0};  /* read = MSB=1 */
    uint8_t rx[2] = {0};
    nrf_drv_spi_transfer(&spi, tx, 2, rx, 2);
    return rx[1];
}

static void imu_read_xyz(float *ax, float *ay, float *az, float *gx, float *gy, float *gz)
{
    uint8_t tx[13] = {ICM_REG_ACCEL_DATA | 0x80, 0};
    uint8_t rx[13] = {0};
    nrf_drv_spi_transfer(&spi, tx, 13, rx, 13);

    int16_t ax_raw = (rx[1] << 8) | rx[2];
    int16_t ay_raw = (rx[3] << 8) | rx[4];
    int16_t az_raw = (rx[5] << 8) | rx[6];
    int16_t gx_raw = (rx[7] << 8) | rx[8];
    int16_t gy_raw = (rx[9] << 8) | rx[10];
    int16_t gz_raw = (rx[11] << 8) | rx[12];

    /* Configured: accel ±4g, gyro ±2000 dps */
    *ax = ax_raw / 8192.0f;
    *ay = ay_raw / 8192.0f;
    *az = az_raw / 8192.0f;
    *gx = gx_raw / 16.4f;
    *gy = gy_raw / 16.4f;
    *gz = gz_raw / 16.4f;
}

static void imu_init(void)
{
    nrf_drv_spi_config_t spi_cfg = NRF_DRV_SPI_DEFAULT_CONFIG;
    spi_cfg.ss_pin = PIN_SPI_CS;
    spi_cfg.mosi_pin = PIN_SPI_MOSI;
    spi_cfg.miso_pin = PIN_SPI_MISO;
    spi_cfg.sck_pin = PIN_SPI_SCK;
    spi_cfg.frequency = NRF_DRV_SPI_FREQ_10M;
    nrf_drv_spi_init(&spi, &spi_cfg, spi_event_handler, NULL);

    nrf_delay_ms(10);

    /* Reset */
    imu_write_reg(ICM_REG_PWR_MGMT0, 0x80); /* software reset */
    nrf_delay_ms(10);

    /* Configure: accel ±4g, gyro ±2000 dps, 200 Hz */
    imu_write_reg(ICM_REG_GYRO_CONFIG0, 0x0C);   /* FS=±2000 dps, ODR=200Hz */
    imu_write_reg(ICM_REG_ACCEL_CONFIG0, 0x0C);  /* FS=±4g, ODR=200Hz */

    /* Enable data ready interrupt */
    imu_write_reg(0x4C, 0x03); /* INT_CONFIG: pulsed, active high */
    imu_write_reg(0x4D, 0x01); /* INT_SOURCE0: data ready */

    /* Turn on */
    imu_write_reg(ICM_REG_PWR_MGMT0, 0x0F); /* LN mode, all sensors on */
    nrf_delay_ms(10);
}

/* ---- I2C helpers ---- */
static void twi_write(uint8_t addr, uint8_t reg, uint8_t val)
{
    uint8_t tx[2] = {reg, val};
    nrf_drv_twi_tx(&twi, addr, tx, 2, false);
}

static uint8_t twi_read(uint8_t addr, uint8_t reg)
{
    uint8_t val = 0;
    nrf_drv_twi_tx(&twi, addr, &reg, 1, true);
    nrf_drv_twi_rx(&twi, addr, &val, 1);
    return val;
}

/* ---- MAX30102 PPG ---- */
static void ppg_init(void)
{
    twi_write(0x57, 0x09, 0x40); /* MODE_CONFIG: heart rate mode */
    twi_write(0x57, 0x0A, 0x1F); /* SPO2_CONFIG: 200Hz, 4112us, 18-bit */
    twi_write(0x57, 0x0C, 0x24); /* LED1_PA (red) */
    twi_write(0x57, 0x0D, 0x24); /* LED2_PA (IR) */
    twi_write(0x57, 0x08, 0x04); /* FIFO_WR_PTR: reset */
    twi_write(0x57, 0x09, 0x40); /* Re-enable HR mode */
}

static void ppg_read(uint8_t *hr, uint8_t *spo2)
{
    /* Read FIFO */
    uint8_t reg = 0x07;
    uint8_t data[6];
    nrf_drv_twi_tx(&twi, 0x57, &reg, 1, true);
    nrf_drv_twi_rx(&twi, 0x57, data, 6);

    /* Simplified HR computation from IR channel */
    int32_t ir = (data[3] << 16) | (data[4] << 8) | data[5];
    int32_t red = (data[0] << 16) | (data[1] << 8) | data[2];

    /* In production: 4-second buffer, FFT, peak detection */
    *hr = 72; /* placeholder */
    *spo2 = 98;
}

/* ---- DRV2605L haptic ---- */
static void haptic_init(void)
{
    twi_write(0x5A, 0x0B, 0x00); /* Mode: internal trigger */
    twi_write(0x5A, 0x01, 0x3F); /* Rated voltage */
    twi_write(0x5A, 0x02, 0x68); /* Overdrive voltage */
    twi_write(0x5A, 0x03, 0x15); /* Negative overdrive */
    twi_write(0x5A, 0x04, 0x00); /* Standby */
}

static void haptic_trigger(uint8_t pattern)
{
    /* Map protocol patterns to DRV2605L waveform library */
    uint8_t wf;
    switch (pattern) {
    case HAPTIC_SINGLE_TAP:   wf = 1;  break;  /* Strong click */
    case HAPTIC_DOUBLE_PULSE: wf = 24; break;  /* Double click */
    case HAPTIC_TRIPLE_BURST: wf = 40; break;  /* Alert */
    case HAPTIC_LONG_BUZZ:    wf = 14; break;  /* Long buzz */
    case HAPTIC_PATTERN_WAVE: wf = 55; break;  /* Pulsing buzz */
    default: wf = 1; break;
    }

    twi_write(0x5A, 0x04, wf);   /* Waveform sequence */
    twi_write(0x5A, 0x0C, 0x01); /* Go */
}

/* ---- MAX17048 fuel gauge ---- */
static uint8_t fuel_gauge_read(void)
{
    uint8_t reg = 0x02; /* VCELL register */
    uint8_t data[2];
    nrf_drv_twi_tx(&twi, 0x36, &reg, 1, true);
    nrf_drv_twi_rx(&twi, 0x36, data, 2);
    uint16_t vcell = ((data[0] << 8) | data[1]) >> 4;
    float voltage = vcell * 1.25f / 1000.0f; /* mV */
    /* Rough SOC: 3.0V=0%, 4.2V=100% */
    int soc = (int)((voltage - 3.0f) / 1.2f * 100.0f);
    if (soc < 0) soc = 0;
    if (soc > 100) soc = 100;
    return (uint8_t)soc;
}

/* ---- Madgwick AHRS filter ---- */
/* Based on Madgwick's IMU AHRS algorithm */
static void madgwick_update(madgwick_state_t *s,
                            float gx, float gy, float gz,
                            float ax, float ay, float az)
{
    float recipNorm;
    float s0, s1, s2, s3;
    float qDot1, qDot2, qDot3, qDot4;
    float _2q0, _2q1, _2q2, _2q3, _4q0, _4q1, _4q2, _4qb3;
    float _8q1, _8q2, q0q0, q1q1, q2q2, q3q3;

    /* Rate of change of quaternion from gyroscope */
    qDot1 = 0.5f * (-s->q1 * gx - s->q2 * gy - s->q3 * gz);
    qDot2 = 0.5f * ( s->q0 * gx + s->q2 * gz - s->q3 * gy);
    qDot3 = 0.5f * ( s->q0 * gy - s->q1 * gz + s->q3 * gx);
    qDot4 = 0.5f * ( s->q0 * gz + s->q1 * gy - s->q2 * gx);

    /* Compute feedback only if accelerometer measurement valid */
    if (!((ax == 0.0f) && (ay == 0.0f) && (az == 0.0f))) {
        recipNorm = 1.0f / sqrtf(ax * ax + ay * ay + az * az);
        ax *= recipNorm;
        ay *= recipNorm;
        az *= recipNorm;

        _2q0 = 2.0f * s->q0;
        _2q1 = 2.0f * s->q1;
        _2q2 = 2.0f * s->q2;
        _2q3 = 2.0f * s->q3;
        _4q0 = 4.0f * s->q0;
        _4q1 = 4.0f * s->q1;
        _4q2 = 4.0f * s->q2;
        _8q1 = 8.0f * s->q1;
        _8q2 = 8.0f * s->q2;
        q0q0 = s->q0 * s->q0;
        q1q1 = s->q1 * s->q1;
        q2q2 = s->q2 * s->q2;
        q3q3 = s->q3 * s->q3;

        /* Gradient decent corrective step */
        s0 = _4q0 * q2q2 + _2q2 * ax + _4q0 * q1q1 - _2q1 * ay;
        s1 = _4q1 * q3q3 - _2q3 * ax + 4.0f * q0q0 * q1 - _2q0 * ay - _4q1 + _8q1 * q1q1 + _8q1 * q2q2 + _4q1 * az;
        s2 = 4.0f * q0q0 * q2 + _2q0 * ax + _4q2 * q3q3 - _2q3 * ay - _4q2 + _8q2 * q1q1 + _8q2 * q2q2 + _4q2 * az;
        s3 = 4.0f * q1q1 * q3 - _2q1 * ax + 4.0f * q2q2 * q3 - _2q2 * ay;

        recipNorm = 1.0f / sqrtf(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3);
        s0 *= recipNorm;
        s1 *= recipNorm;
        s2 *= recipNorm;
        s3 *= recipNorm;

        /* Apply feedback step */
        qDot1 -= s->beta * s0;
        qDot2 -= s->beta * s1;
        qDot3 -= s->beta * s2;
        qDot4 -= s->beta * s3;
    }

    /* Integrate rate of change of quaternion */
    s->q0 += qDot1 * s->sample_dt;
    s->q1 += qDot2 * s->sample_dt;
    s->q2 += qDot3 * s->sample_dt;
    s->q3 += qDot4 * s->sample_dt;

    /* Normalise quaternion */
    recipNorm = 1.0f / sqrtf(s->q0 * s->q0 + s->q1 * s->q1 + s->q2 * s->q2 + s->q3 * s->q3);
    s->q0 *= recipNorm;
    s->q1 *= recipNorm;
    s->q2 *= recipNorm;
    s->q3 *= recipNorm;
}

/* Get Euler angles from quaternion */
static void ahrs_get_euler(const madgwick_state_t *s, float *pitch, float *roll, float *yaw)
{
    /* Tait-Bryan angles (rotation order: ZYX) */
    *roll  = atan2f(2.0f * (s->q0 * s->q1 + s->q2 * s->q3), 1.0f - 2.0f * (s->q1 * s->q1 + s->q2 * s->q2));
    *pitch = asinf(2.0f * (s->q0 * s->q2 - s->q3 * s->q1));
    *yaw   = atan2f(2.0f * (s->q0 * s->q3 + s->q1 * s->q2), 1.0f - 2.0f * (s->q2 * s->q2 + s->q3 * s->q3));

    *pitch *= 180.0f / 3.14159265f;
    *roll  *= 180.0f / 3.14159265f;
    *yaw   *= 180.0f / 3.14159265f;
}

/* ---- Posture classification (simple threshold-based for edge) ---- */
static uint8_t classify_posture(float pitch, float roll)
{
    if (!g_cal.calibrated) return POSTURE_NEUTRAL;

    float adj_pitch = pitch - g_cal.pitch_offset;
    float adj_roll = roll - g_cal.roll_offset;

    if (adj_pitch > 20.0f) return POSTURE_SLOUCHING;
    if (adj_pitch > 15.0f) return POSTURE_FORWARD_HEAD;
    if (adj_pitch < -10.0f) return POSTURE_HYPEREXTENSION;
    if (adj_roll > 5.0f) return POSTURE_LATERAL_LEFT;
    if (adj_roll < -5.0f) return POSTURE_LATERAL_RIGHT;
    if (adj_pitch > 10.0f && adj_roll > 3.0f) return POSTURE_KYPHOTIC;
    if (adj_pitch < -5.0f && adj_roll > 3.0f) return POSTURE_LORDOTIC;
    if (fabsf(adj_roll) > 7.0f) return POSTURE_SCOLIOTIC;
    if (adj_pitch > 12.0f) return POSTURE_ANTERIOR_TILT;
    if (adj_pitch < -8.0f) return POSTURE_POSTERIOR_TILT;

    return POSTURE_NEUTRAL;
}

/* ---- BLE GATT service ---- */
/* PostureSync Service UUID: custom 128-bit */
static ble_uuid_t g_service_uuid = {
    .uuid = POSTSYNC_SERVICE_UUID,
    .type = BLE_UUID_TYPE_VENDOR_BEGIN + 1
};

static ble_gatts_char_handles_t g_spine_angle_handles;
static ble_gatts_char_handles_t g_ppg_handles;
static ble_gatts_char_handles_t g_battery_handles;
static ble_gatts_char_handles_t g_haptic_handles;
static ble_gatts_char_handles_t g_cal_handles;

static void ble_add_characteristic(uint16_t uuid, ble_gatts_char_handles_t *handles,
                                    bool notify, bool write)
{
    ble_gatts_char_md_t char_md = {0};
    ble_gatts_attr_md_t cccd_md = {0};
    ble_gatts_attr_t    attr_char_value = {0};
    ble_uuid_t          char_uuid = { .uuid = uuid, .type = g_service_uuid.type };
    ble_gatts_attr_md_t attr_md = {0};

    if (notify) {
        char_md.char_props.notify = 1;
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);
        cccd_md.vloc = BLE_GATTS_VLOC_STACK;
        char_md.p_cccd_md = &cccd_md;
    }
    if (write) {
        char_md.char_props.write = 1;
    }

    attr_md.vloc = BLE_GATTS_VLOC_STACK;
    attr_md.vlen = 1;
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);

    attr_char_value.p_uuid = &char_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len = 20;
    attr_char_value.max_len = 32;

    sd_ble_gatts_characteristic_add(BLE_GATT_HANDLE_INVALID, &char_md, &attr_char_value, handles);
}

static void ble_service_init(void)
{
    /* Register custom service UUID */
    ble_uuid128_t base_uuid = {{0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
                                 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    sd_ble_uuid_vs_add(&base_uuid, &g_service_uuid.type);

    ble_gatts_service_md_t service_md = {0};
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&service_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&service_md.write_perm);
    service_md.uuid = g_service_uuid;
    service_md.uuid.type = g_service_uuid.type;

    sd_ble_gatts_service_add(BLE_GATT_HANDLE_INVALID, &service_md, &g_gatts);

    /* Add characteristics */
    ble_add_characteristic(0x5350, &g_spine_angle_handles, true, false);  /* P5S1: spine angle */
    ble_add_characteristic(0x5353, &g_ppg_handles, true, false);         /* P5S3: PPG */
    ble_add_characteristic(0x5357, &g_battery_handles, true, false);      /* P5S7: battery */
    ble_add_characteristic(0x5355, &g_haptic_handles, false, true);       /* P5S5: haptic cmd */
    ble_add_characteristic(0x5358, &g_cal_handles, false, true);          /* P5S8: calibration */
}

static void ble_send_spine_angle(float pitch, float roll, float yaw, uint8_t posture, uint8_t hr)
{
    if (g_conn_handle == BLE_CONN_HANDLE_INVALID) return;

    spine_band_data_t data = {0};
    data.pitch = pitch;
    data.roll = roll;
    data.yaw = yaw;
    data.posture_class = posture;
    data.hr = hr;
    data.spo2 = g_spo2;
    data.battery = g_battery;
    data.flags = g_cal.calibrated ? 0x03 : 0x00;

    ble_gatts_hvx_params_t hvx = {0};
    uint16_t len = sizeof(data);
    hvx.handle = g_spine_angle_handles.value_handle;
    hvx.type = BLE_GATT_HVX_NOTIFICATION;
    hvx.p_data = (uint8_t *)&data;
    hvx.p_len = &len;
    sd_ble_gatts_hvx(g_conn_handle, &hvx);
}

/* ---- BLE event handler ---- */
static void ble_evt_handler(ble_evt_t const *p_ble_evt, void *p_context)
{
    switch (p_ble_evt->header.evt_id) {
    case BLE_GAP_EVT_CONNECTED:
        g_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
        g_ble_connected = true;
        nrf_gpio_pin_write(PIN_LED_G, 1);
        NRF_LOG_INFO("BLE connected");
        break;

    case BLE_GAP_EVT_DISCONNECTED:
        g_conn_handle = BLE_CONN_HANDLE_INVALID;
        g_ble_connected = false;
        nrf_gpio_pin_write(PIN_LED_G, 0);
        NRF_LOG_INFO("BLE disconnected");
        break;

    case BLE_GATTS_EVT_WRITE:
        {
            ble_gatts_evt_write_t *w = &p_ble_evt->evt.gatts_evt.params.write;
            if (w->handle == g_haptic_handles.value_handle && w->len > 0) {
                haptic_trigger(w->data[0]);
            }
            if (w->handle == g_cal_handles.value_handle && w->len > 0) {
                /* Start calibration */
                g_cal.calibrated = false;
                NRF_LOG_INFO("Calibration requested");
            }
        }
        break;

    default:
        break;
    }
}

/* ---- Sampling timer callback ---- */
static void sample_timer_handler(void *p_context)
{
    /* Read IMU */
    float ax, ay, az, gx, gy, gz;
    imu_read_xyz(&ax, &ay, &az, &gx, &gy, &gz);

    /* Update AHRS */
    madgwick_update(&g_ahrs, gx, gy, gz, ax, ay, az);

    /* Get Euler angles */
    float pitch, roll, yaw;
    ahrs_get_euler(&g_ahrs, &pitch, &roll, &yaw);

    /* Classify posture */
    g_posture_class = classify_posture(pitch, roll);

    /* Store for BLE TX */
    g_imu.ax = ax; g_imu.ay = ay; g_imu.az = az;
    g_imu.gx = gx; g_imu.gy = gy; g_imu.gz = gz;
}

/* ---- BLE TX timer (10 Hz) ---- */
static void ble_tx_timer_handler(void *p_context)
{
    if (!g_ble_connected) return;

    float pitch, roll, yaw;
    ahrs_get_euler(&g_ahrs, &pitch, &roll, &yaw);

    /* Read PPG (throttled) */
    static uint8_t ppg_counter = 0;
    if (++ppg_counter >= 10) { /* 1 Hz PPG read */
        ppg_read(&g_hr, &g_spo2);
        ppg_counter = 0;
    }

    /* Read fuel gauge (throttled) */
    static uint8_t bat_counter = 0;
    if (++bat_counter >= 60) { /* 1/min battery */
        g_battery = fuel_gauge_read();
        bat_counter = 0;
    }

    /* Send spine angle data via BLE */
    ble_send_spine_angle(pitch, roll, yaw, g_posture_class, g_hr);

    /* Auto-correction haptic */
    static uint8_t poor_posture_count = 0;
    if (g_posture_class == POSTURE_FORWARD_HEAD || g_posture_class == POSTURE_SLOUCHING) {
        poor_posture_count++;
        if (poor_posture_count >= 30) { /* 3 seconds at 10Hz */
            haptic_trigger(HAPTIC_DOUBLE_PULSE);
            poor_posture_count = 0;
        }
    } else {
        poor_posture_count = 0;
    }
}

/* ---- Calibration ---- */
static void run_calibration(void)
{
    NRF_LOG_INFO("Calibration: stand neutral against wall");
    nrf_gpio_pin_write(PIN_LED_B, 1);

    /* Collect 2 seconds of data at neutral */
    float pitch_sum = 0, roll_sum = 0, yaw_sum = 0;
    for (int i = 0; i < 400; i++) { /* 2 seconds at 200Hz */
        float ax, ay, az, gx, gy, gz;
        imu_read_xyz(&ax, &ay, &az, &gx, &gy, &gz);
        madgwick_update(&g_ahrs, gx, gy, gz, ax, ay, az);
        float p, r, y;
        ahrs_get_euler(&g_ahrs, &p, &r, &y);
        pitch_sum += p; roll_sum += r; yaw_sum += y;
        nrf_delay_ms(SAMPLE_PERIOD_MS);
    }
    g_cal.pitch_offset = pitch_sum / 400.0f;
    g_cal.roll_offset = roll_sum / 400.0f;
    g_cal.yaw_offset = yaw_sum / 400.0f;

    NRF_LOG_INFO("Neutral: pitch=%.1f roll=%.1f", g_cal.pitch_offset, g_cal.roll_offset);

    /* Forward limit */
    NRF_LOG_INFO("Bend forward 30 degrees");
    nrf_delay_ms(3000);
    float p, r, y;
    ahrs_get_euler(&g_ahrs, &p, &r, &y);
    g_cal.pitch_forward_limit = p - g_cal.pitch_offset;

    /* Backward limit */
    NRF_LOG_INFO("Lean backward 10 degrees");
    nrf_delay_ms(3000);
    ahrs_get_euler(&g_ahrs, &p, &r, &y);
    g_cal.pitch_backward_limit = p - g_cal.pitch_offset;

    /* Lateral limits */
    NRF_LOG_INFO("Lean left 20 degrees");
    nrf_delay_ms(3000);
    ahrs_get_euler(&g_ahrs, &p, &r, &y);
    g_cal.roll_left_limit = r - g_cal.roll_offset;

    NRF_LOG_INFO("Lean right 20 degrees");
    nrf_delay_ms(3000);
    ahrs_get_euler(&g_ahrs, &p, &r, &y);
    g_cal.roll_right_limit = r - g_cal.roll_offset;

    g_cal.calibrated = true;
    nrf_gpio_pin_write(PIN_LED_B, 0);
    nrf_gpio_pin_write(PIN_LED_G, 1);
    NRF_LOG_INFO("Calibration complete!");
}

/* ---- Main ---- */
int main(void)
{
    /* Logging */
    NRF_LOG_INIT(NULL);
    NRF_LOG_DEFAULT_BACKENDS_INIT();

    NRF_LOG_INFO("PostureSync Spine Band starting...");

    /* App timer */
    app_timer_init();

    /* I2C init */
    nrf_drv_twi_config_t twi_cfg = NRF_DRV_TWI_DEFAULT_CONFIG;
    twi_cfg.sda = PIN_I2C_SDA;
    twi_cfg.scl = PIN_I2C_SCL;
    twi_cfg.frequency = NRF_TWIM_FREQ_400K;
    nrf_drv_twi_init(&twi, &twi_cfg, NULL, NULL);
    nrf_drv_twi_enable(&twi);

    /* GPIO */
    nrf_gpio_cfg_output(PIN_LED_R);
    nrf_gpio_cfg_output(PIN_LED_G);
    nrf_gpio_cfg_output(PIN_LED_B);
    nrf_gpio_cfg_output(PIN_HAPTIC_EN);
    nrf_gpio_cfg_input(PIN_BTN_PAIR, NRF_GPIO_PIN_PULLUP);

    /* Init sensors */
    imu_init();
    ppg_init();
    haptic_init();

    /* BLE init */
    nrf_sdh_enable_request();
    nrf_sdh_ble_default_cfg_set(BLE_CONN_CFG_TAG, 1);
    nrf_sdh_ble_enable(NULL);
    ble_service_init();
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);

    /* Advertising — simplified */
    /* In production: configure advertising data with service UUID */

    /* Start sampling timer (200 Hz) */
    app_timer_create(&g_sample_timer, APP_TIMER_MODE_REPEATED, sample_timer_handler);
    app_timer_start(g_sample_timer, APP_TIMER_TICKS(SAMPLE_PERIOD_MS), NULL);

    /* Start BLE TX timer (10 Hz = 100ms) */
    app_timer_create(&g_ble_tx_timer, APP_TIMER_MODE_REPEATED, ble_tx_timer_handler);
    app_timer_start(g_ble_tx_timer, APP_TIMER_TICKS(100), NULL);

    /* Check for calibration button press */
    if (nrf_gpio_pin_read(PIN_BTN_PAIR) == 0) {
        run_calibration();
    }

    NRF_LOG_INFO("Spine Band running");

    while (1) {
        __WFE();
        NRF_LOG_FLUSH();
    }
}