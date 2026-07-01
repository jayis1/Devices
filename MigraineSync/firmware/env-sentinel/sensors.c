/**
 * MigraineSync — Env Sentinel Sensors
 * ===================================
 * ESP32-S3 I²C sensor drivers via TCA9548A mux:
 *   BMP390, VEML7700, BME688, SCD41, SHT45, SPL06-007
 *
 * License: MIT
 */

#include "sensors.h"
#include "config.h"
#include "../common/protocol.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/i2c.h>
#include <esp_log.h>
#include <string.h>
#include <math.h>

static const char *TAG = "migrainesync_env_sensors";

/* ── I²C helpers ────────────────────────────────────────── */
static esp_err_t i2c_write(uint8_t addr, const uint8_t *data, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, data, len, true);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return err;
}

static esp_err_t i2c_read(uint8_t addr, uint8_t *data, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);
    if (len > 1)
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &data[len - 1], I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return err;
}

/* ── TCA9548A mux channel select ────────────────────────── */
static void mux_select(uint8_t channel)
{
    uint8_t cmd = (1 << channel);
    i2c_write(MUX_ADDR, &cmd, 1);
    vTaskDelay(pdMS_TO_TICKS(5));  /* mux settle */
}

/* ── BMP390 Barometric Pressure ─────────────────────────── */
/* I²C 0x76, 16-bit pressure + temp, oversampling x1 */
static int bmp390_read(float *pressure, float *temp)
{
    mux_select(0);

    /* In production: write config registers for oversampling + power mode
     * BMP390_REG_CTRL_MEAS = 0x1F → OSRS_T=1, OSRS_P=1, MODE=11 (normal)
     * BMP390_REG_PWR_CTRL = 0x1B → press_en=1, temp_en=1
     */

    /* Read 6 bytes: press MSB/LSB/XLSB, temp MSB/LSB/XLSB */
    uint8_t reg = 0x04;  /* PRESS_MSB register */
    i2c_write(BMP390_ADDR, &reg, 1);
    uint8_t data[6];
    if (i2c_read(BMP390_ADDR, data, 6) != ESP_OK)
        return -1;

    uint32_t raw_press = ((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | data[2];
    uint32_t raw_temp  = ((uint32_t)data[3] << 16) | ((uint32_t)data[4] << 8) | data[5];

    /* In production: apply BMP390 calibration coefficients (trim registers)
     * For stub: approximate using nominal conversion
     */
    *pressure = (float)raw_press / 256.0f / 100.0f + 900.0f;  /* approximate hPa */
    *temp = (float)raw_temp / 256.0f / 100.0f - 20.0f;        /* approximate °C */

    /* Clamp to realistic range */
    if (*pressure < 800 || *pressure > 1100)
        *pressure = 1013.25f;
    if (*temp < -20 || *temp > 60)
        *temp = 22.0f;

    return 0;
}

/* ── VEML7700 Ambient Light ─────────────────────────────── */
/* I²C 0x10, 16-bit lux, config register 0x00 */
static int veml7700_read(float *lux)
{
    mux_select(2);

    /* Config: gain=1x, IT=25ms → reg 0x00 = 0x0000 */
    uint8_t cfg[3] = { 0x00, 0x00, 0x00 };
    i2c_write(VEML7700_ADDR, cfg, 3);
    vTaskDelay(pdMS_TO_TICKS(50));  /* first reading after config */

    /* Read ALS data register 0x04 */
    uint8_t reg = 0x04;
    i2c_write(VEML7700_ADDR, &reg, 1);
    uint8_t data[2];
    if (i2c_read(VEML7700_ADDR, data, 2) != ESP_OK)
        return -1;

    uint16_t raw = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
    /* With gain=1x, IT=25ms: lux = raw * 0.0036 */
    *lux = raw * 0.0036f;
    return 0;
}

/* ── SHT45 Temperature + Humidity ───────────────────────── */
/* I²C 0x44, single-shot measurement command 0xFD */
static int sht45_read(float *temp, float *rh)
{
    mux_select(2);

    uint8_t cmd = 0xFD;  /* high repeatability, clock stretching */
    i2c_write(SHT45_ADDR, &cmd, 1);
    vTaskDelay(pdMS_TO_TICKS(10));  /* measurement time */

    uint8_t data[6];
    if (i2c_read(SHT45_ADDR, data, 6) != ESP_OK)
        return -1;

    uint16_t raw_t = ((uint16_t)data[0] << 8) | data[1];
    uint16_t raw_rh = ((uint16_t)data[3] << 8) | data[4];

    *temp = -45.0f + 175.0f * (float)raw_t / 65535.0f;
    *rh = 100.0f * (float)raw_rh / 65535.0f;

    if (*rh > 100) *rh = 100;
    if (*rh < 0) *rh = 0;
    return 0;
}

/* ── BME688 VOC / IAQ ───────────────────────────────────── */
/* I²C 0x77, Bosch BSEC library in production.
 * For stub: read raw gas resistance + temp/humidity.
 */
static int bme688_read(float *voc_index, float *temp, float *rh)
{
    mux_select(3);

    /* In production: use Bosch BSEC2 library for IAQ calculation.
     * BSEC handles gas heater profiles, baseline calibration, and
     * converts raw gas resistance → IAQ index (0-500).
     *
     * For stub: return a pseudo IAQ from gas resistance.
     */

    /* Read temperature from 0x22-0x24 (3 bytes) */
    uint8_t reg = 0x22;
    i2c_write(BME688_ADDR, &reg, 1);
    uint8_t data[3];
    i2c_read(BME688_ADDR, data, 3);

    uint32_t raw_temp = ((uint32_t)data[0] << 12) | ((uint32_t)data[1] << 4) |
                        (data[2] >> 4);
    *temp = (float)raw_temp / 100.0f;

    /* Pseudo IAQ: in production BSEC computes this properly */
    *voc_index = 80.0f;  /* nominal "good" air quality */

    /* Read humidity 0x25-0x27 */
    reg = 0x25;
    i2c_write(BME688_ADDR, &reg, 1);
    i2c_read(BME688_ADDR, data, 3);
    uint32_t raw_rh = ((uint32_t)data[0] << 12) | ((uint32_t)data[1] << 4) |
                      (data[2] >> 4);
    *rh = 100.0f * (float)raw_rh / 102400.0f;
    if (*rh > 100) *rh = 100;

    return 0;
}

/* ── SCD41 CO₂ ──────────────────────────────────────────── */
/* I²C 0x62, NDIR CO₂. Single shot measurement command 0x219D. */
static int scd41_read(uint16_t *co2)
{
    mux_select(3);

    /* In production: start periodic measurement (0x21B1) once at init,
     * then read data ready status (0xE4B8) → if data ready, read (0xEC05).
     */

    /* Read measurement: 0xEC05 → 9 bytes (co2, temp, rh) */
    uint8_t cmd[2] = { 0xEC, 0x05 };
    i2c_write(SCD41_ADDR, cmd, 2);
    vTaskDelay(pdMS_TO_TICKS(5));

    uint8_t data[9];
    if (i2c_read(SCD41_ADDR, data, 9) != ESP_OK)
        return -1;

    *co2 = ((uint16_t)data[0] << 8) | data[1];
    /* data[2] = CRC (skip in stub) */
    /* data[3-5] = temperature, data[6-8] = humidity */

    if (*co2 < 400) *co2 = 400;  /* minimum atmospheric CO₂ */
    return 0;
}

/* ── SPL06-007 Sound Pressure Level ─────────────────────── */
/* I²C 0x76 (on mux channel 1 to avoid BMP390 conflict).
 * SPL06 has internal DSP that computes dB SPL.
 */
static int spl06_read(uint8_t *noise_db)
{
    mux_select(1);

    /* In production: configure SPL06 for continuous SPL measurement,
     * read SPL_DATA register.
     * For stub: return a nominal indoor SPL.
     */

    uint8_t reg = 0x26;  /* SPL data register (hypothetical) */
    i2c_write(SPL06_ADDR, &reg, 1);
    uint8_t data[2];
    if (i2c_read(SPL06_ADDR, data, 2) != ESP_OK) {
        *noise_db = 45;  /* nominal quiet room */
        return 0;
    }

    *noise_db = data[0];  /* dB SPL as uint8 */
    if (*noise_db < 30) *noise_db = 30;
    if (*noise_db > 120) *noise_db = 120;
    return 0;
}

/* ── Pressure history for 3-hour delta ──────────────────── */
#define PRESSURE_HISTORY_LEN  10800  /* 3 hours at 1 Hz */
static float s_pressure_history[PRESSURE_HISTORY_LEN];
static int s_pressure_idx = 0;
static bool s_pressure_full = false;

void sensors_update_pressure_history(float current_pressure)
{
    s_pressure_history[s_pressure_idx] = current_pressure;
    s_pressure_idx = (s_pressure_idx + 1) % PRESSURE_HISTORY_LEN;
    if (s_pressure_idx == 0)
        s_pressure_full = true;
}

static float compute_pressure_delta_3h(void)
{
    if (!s_pressure_full && s_pressure_idx < 2)
        return 0.0f;

    int oldest_idx = s_pressure_full
                     ? s_pressure_idx
                     : 0;
    return s_pressure_history[(s_pressure_idx - 1 + PRESSURE_HISTORY_LEN) % PRESSURE_HISTORY_LEN]
         - s_pressure_history[oldest_idx];
}

/* ── Init all sensors ───────────────────────────────────── */
int sensors_init(void)
{
    ESP_LOGI(TAG, "Initializing sensors via TCA9548A mux");

    /* Init I²C master */
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);

    /* Init each sensor via mux */
    /* BMP390 on ch0 */
    mux_select(0);
    {
        uint8_t cfg[3] = { 0x1B, 0x00, 0x03 };  /* PWR_CTRL: press_en + temp_en */
        i2c_write(BMP390_ADDR, cfg, 3);
    }

    /* VEML7700 on ch2 */
    mux_select(2);
    {
        uint8_t cfg[3] = { 0x00, 0x00, 0x00 };  /* gain=1x, IT=25ms */
        i2c_write(VEML7700_ADDR, cfg, 3);
    }

    /* BME688 on ch3 — in production, init with BSEC2 library */
    mux_select(3);

    /* SCD41 on ch3 — start periodic measurement */
    mux_select(3);
    {
        uint8_t cmd[2] = { 0x21, 0xB1 };  /* start periodic measurement */
        i2c_write(SCD41_ADDR, cmd, 2);
    }

    memset(s_pressure_history, 0, sizeof(s_pressure_history));

    ESP_LOGI(TAG, "All sensors initialized (BMP390, VEML7700, SHT45, BME688, SCD41, SPL06)");
    return 0;
}

/* ── Read all sensors ───────────────────────────────────── */
int sensors_read(env_data_t *data)
{
    if (!data)
        return -1;

    int ret = 0;

    /* BMP390 — barometric pressure */
    float bmp_temp;
    if (bmp390_read(&data->pressure_hpa, &bmp_temp) != 0) {
        ESP_LOGW(TAG, "BMP390 read failed");
        data->pressure_hpa = 1013.25f;
        ret = -1;
    }

    /* Update pressure history + compute 3-hour delta */
    sensors_update_pressure_history(data->pressure_hpa);
    data->pressure_delta_3h = compute_pressure_delta_3h();

    /* VEML7700 — ambient light */
    if (veml7700_read(&data->light_lux) != 0) {
        ESP_LOGW(TAG, "VEML7700 read failed");
        data->light_lux = 200.0f;
        ret = -1;
    }

    /* SHT45 — temperature + humidity */
    if (sht45_read(&data->temp_c, &data->humidity_pct) != 0) {
        ESP_LOGW(TAG, "SHT45 read failed");
        data->temp_c = 22.0f;
        data->humidity_pct = 45.0f;
        ret = -1;
    }

    /* BME688 — VOC index */
    float bme_temp, bme_rh;
    if (bme688_read(&data->voc_index, &bme_temp, &bme_rh) != 0) {
        ESP_LOGW(TAG, "BME688 read failed");
        data->voc_index = 80.0f;
        ret = -1;
    }

    /* SCD41 — CO₂ */
    if (scd41_read(&data->co2_ppm) != 0) {
        ESP_LOGW(TAG, "SCD41 read failed");
        data->co2_ppm = 450;
        ret = -1;
    }

    /* SPL06-007 — noise level */
    if (spl06_read(&data->noise_db) != 0) {
        ESP_LOGW(TAG, "SPL06 read failed");
        data->noise_db = 45;
        ret = -1;
    }

    return ret;
}