/**
 * JointSync Compression Sleeve — PID Pressure Controller
 *
 * BMP390 barometric pressure + NAU7802 load cell.
 * PID loop at 10 Hz (100 ms cycle).
 *
 * License: MIT
 */

#include "pressure_control.h"
#include "esp_log.h"
#include "driver/i2c.h"

static const char *TAG = "pressure_ctrl";

#define I2C_PORT        I2C_NUM_0
#define BMP390_ADDR     0x77
#define NAU7802_ADDR    0x2A

/* PID state */
static pid_t g_pid = {0};

/* BMP390 calibration data */
static int16_t dig_P1, dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
static int32_t t_fine;

/* ── BMP390 I²C Helpers ──────────────────────────────────────────── */

static void bmp390_write(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    i2c_master_write_to_device(I2C_PORT, BMP390_ADDR, buf, 2, pdMS_TO_TICKS(100));
}

static uint8_t bmp390_read(uint8_t reg)
{
    uint8_t val;
    i2c_master_write_read_device(I2C_PORT, BMP390_ADDR, &reg, 1, &val, 1, pdMS_TO_TICKS(100));
    return val;
}

static void bmp390_read_multi(uint8_t reg, uint8_t *buf, uint8_t len)
{
    i2c_master_write_read_device(I2C_PORT, BMP390_ADDR, &reg, 1, buf, len, pdMS_TO_TICKS(100));
}

/* ── NAU7802 I²C Helpers ──────────────────────────────────────────── */

static void nau7802_write(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    i2c_master_write_to_device(I2C_PORT, NAU7802_ADDR, buf, 2, pdMS_TO_TICKS(100));
}

static uint8_t nau7802_read(uint8_t reg)
{
    uint8_t val;
    i2c_master_write_read_device(I2C_PORT, NAU7802_ADDR, &reg, 1, &val, 1, pdMS_TO_TICKS(100));
    return val;
}

/* ── Public API ───────────────────────────────────────────────────── */

void pressure_control_init(void)
{
    /* Initialize I²C */
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = 10,
        .scl_io_num = 9,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    i2c_param_config(I2C_PORT, &conf);
    i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0);

    /* Initialize BMP390 */
    /* Read calibration coefficients */
    uint8_t calib[20];
    bmp390_read_multi(0x10, calib, 20);
    dig_P1 = (int16_t)((calib[0] | (calib[1] << 8)));
    dig_P2 = (int16_t)((calib[2] | (calib[3] << 8)));
    dig_P3 = (int16_t)((calib[4] | (calib[5] << 8)));
    dig_P4 = (int16_t)((calib[6] | (calib[7] << 8)));
    dig_P5 = (int16_t)((calib[8] | (calib[9] << 8)));
    dig_P6 = (int16_t)((calib[10] | (calib[11] << 8)));
    dig_P7 = (int16_t)((calib[12] | (calib[13] << 8)));
    dig_P8 = (int16_t)((calib[14] | (calib[15] << 8)));
    dig_P9 = (int16_t)((calib[16] | (calib[17] << 8)));

    /* Configure: oversampling x8, normal mode */
    bmp390_write(0x1C, 0x0B);  /* Ctrl_meas: temp x8, pressure x8 */
    bmp390_write(0x1D, 0x30);  /* Config: IIR filter coefficient 4 */
    bmp390_write(0x1E, 0x01);  /* Power control: normal mode */

    /* Initialize NAU7802 (24-bit ADC for load cell) */
    nau7802_write(0x00, 0x0E);  /* Reset */
    vTaskDelay(pdMS_TO_TICKS(1));
    nau7802_write(0x00, 0x07);  /* Power on, PGA gain 128 */
    nau7802_write(0x01, 0x20);  /* Select channel 1, SPS 80 */

    /* Initialize PID */
    g_pid.kp = 2.0f;
    g_pid.ki = 0.5f;
    g_pid.kd = 0.1f;
    g_pid.integral = 0.0f;
    g_pid.prev_error = 0.0f;

    ESP_LOGI(TAG, "Pressure control initialized (BMP390 + NAU7802, PID kp=2.0 ki=0.5 kd=0.1)");
}

float pressure_control_read(void)
{
    /* Read BMP390 pressure registers */
    uint8_t data[3];
    bmp390_read_multi(0x04, data, 3);
    int32_t raw_pressure = ((int32_t)data[0] << 12) | ((int32_t)data[1] << 4) | (data[2] >> 4);

    /* Read temperature for compensation */
    bmp390_read_multi(0x07, data, 3);
    int32_t raw_temp = ((int32_t)data[0] << 12) | ((int32_t)data[1] << 4) | (data[2] >> 4);

    /* BMP390 compensation (simplified) */
    int64_t var1, var2, p;
    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)dig_P6;
    var2 = var2 + ((var1 * (int64_t)dig_P5) << 17);
    var2 = var2 + (((int64_t)dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)dig_P3) >> 8) + ((var1 * (int64_t)dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)dig_P1) >> 33;
    if (var1 == 0) return 0;

    p = 1048576 - raw_pressure;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)dig_P7) << 4);

    /* Convert Pa to mmHg */
    float pressure_pa = (float)p / 256.0f;
    float pressure_mmhg = pressure_pa / 133.322f;

    /* Subtract atmospheric pressure to get gauge pressure (bladder pressure) */
    /* Standard atmospheric: 760 mmHg → gauge = absolute - 760 */
    float gauge_mmhg = pressure_mmhg - 760.0f;
    if (gauge_mmhg < 0) gauge_mmhg = 0;

    return gauge_mmhg;
}

int16_t pressure_control_read_loadcell(void)
{
    /* Read NAU7802 24-bit ADC */
    /* Check if data ready */
    uint8_t ctrl = nau7802_read(0x00);
    if (!(ctrl & 0x20)) return 0;  /* DR bit not set */

    uint8_t data[3];
    i2c_master_write_read_device(I2C_PORT, NAU7802_ADDR, (uint8_t[]){0x12}, 1, data, 3, pdMS_TO_TICKS(100));
    int32_t raw = ((int32_t)data[0] << 16) | ((int32_t)data[1] << 8) | data[2];
    /* Sign extend from 24-bit */
    if (raw & 0x800000) raw |= 0xFF000000;

    return (int16_t)(raw >> 8);  /* Simplified to 16-bit */
}

float pressure_control_pid(float setpoint, float measured)
{
    const float dt = 0.1f;  /* 100 ms cycle */
    float error = setpoint - measured;

    g_pid.integral += error * dt;
    /* Anti-windup */
    if (g_pid.integral > 50.0f) g_pid.integral = 50.0f;
    if (g_pid.integral < -50.0f) g_pid.integral = -50.0f;

    float derivative = (error - g_pid.prev_error) / dt;
    float output = g_pid.kp * error + g_pid.ki * g_pid.integral + g_pid.kd * derivative;
    g_pid.prev_error = error;

    /* Clamp output to [0, 1] for PWM duty */
    if (output > 1.0f) output = 1.0f;
    if (output < 0.0f) output = 0.0f;

    return output;
}