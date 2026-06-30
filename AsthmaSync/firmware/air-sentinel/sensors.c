/**
 * AsthmaSync — Air Sentinel Sensor Drivers
 * ========================================
 * I²C drivers for:
 *   - PMSA003I (Plantower PM1.0/2.5/10)
 *   - BME688 (Bosch VOC/IAQ, temp, humidity, pressure)
 *   - SGP40 (Sensirion VOC index / HCHO equivalent)
 *   - SCD41 (Sensirion NDIR CO₂)
 *
 * License: MIT
 */

#include "config.h"
#include "../common/protocol.h"
#include "sensors.h"
#include <string.h>
#include <math.h>

/* ── ESP-IDF includes ──────────────────────────────────── */
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "asthmasync.air.sensors";

/* ── I²C Helper ────────────────────────────────────────── */
static int i2c_write(uint8_t addr, const uint8_t *data, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, data, len, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return (ret == ESP_OK) ? 0 : -1;
}

static int i2c_read(uint8_t addr, uint8_t *data, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);
    if (len > 1)
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &data[len - 1], I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return (ret == ESP_OK) ? 0 : -1;
}

static int i2c_write_reg(uint8_t addr, uint8_t reg, const uint8_t *val, size_t len)
{
    uint8_t buf[16];
    buf[0] = reg;
    memcpy(&buf[1], val, len);
    return i2c_write(addr, buf, len + 1);
}

static int i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *data, size_t len)
{
    if (i2c_write(addr, &reg, 1) != 0)
        return -1;
    return i2c_read(addr, data, len);
}

/* ── PMSA003I ──────────────────────────────────────────── */
int pmsa003i_init(void)
{
    /* Wake sensor (SET pin HIGH) */
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << PMSA_SET_PIN) | (1ULL << PMSA_RST_PIN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io);
    gpio_set_level(PMSA_RST_PIN, 1);
    gpio_set_level(PMSA_SET_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "PMSA003I initialized");
    return 0;
}

int pmsa003i_read(pmsa_data_t *out)
{
    /* PMSA003I: read 32 bytes starting at register 0x00 */
    uint8_t data[32];
    if (i2c_read(ADDR_PMSA003I, data, 32) != 0)
        return -1;

    /* Check start bytes */
    if (data[0] != 0x42 || data[1] != 0x4D)
        return -2;

    /* Checksum: sum of first 30 bytes, compare with last 2 */
    uint16_t checksum = 0;
    for (int i = 0; i < 30; i++)
        checksum += data[i];
    uint16_t expected = (data[30] << 8) | data[31];
    if (checksum != expected)
        return -3;

    /* PM1.0 standard = bytes 4-5, PM2.5 standard = bytes 6-7, PM10 = bytes 8-9 */
    out->pm1_0_ug  = (data[4]  << 8) | data[5];
    out->pm2_5_ug  = (data[6]  << 8) | data[7];
    out->pm10_ug   = (data[8]  << 8) | data[9];
    out->pm1_0_raw = (data[10] << 8) | data[11];
    out->pm2_5_raw = (data[12] << 8) | data[13];
    out->pm10_raw  = (data[14] << 8) | data[15];
    return 0;
}

/* ── BME688 ────────────────────────────────────────────── */
static uint8_t s_bme_calib[41];

int bme688_init(void)
{
    /* Read calibration data */
    if (i2c_read_reg(ADDR_BME688, 0x33, s_bme_calib, 23) != 0)
        return -1;
    if (i2c_read_reg(ADDR_BME688, 0x50, s_bme_calib + 23, 18) != 0)
        return -1;

    /* Configure humidity: x1 oversampling */
    uint8_t ctrl_hum = 0x01;
    i2c_write_reg(ADDR_BME688, 0xF2, &ctrl_hum, 1);

    /* Configure temperature + pressure: x2 oversampling each */
    uint8_t ctrl_meas = (0x02 << 5) | (0x02 << 2) | 0x01; /* x2, x2, forced mode */
    i2c_write_reg(ADDR_BME688, 0xF4, &ctrl_meas, 1);

    ESP_LOGI(TAG, "BME688 initialized");
    return 0;
}

int bme688_read(bme688_data_t *out)
{
    uint8_t data[8];
    /* Read press(msb)-press(lsb)-press(xlsb)-temp(msb)-temp(lsb)-temp(xlsb)-hum(msb)-hum(lsb) */
    if (i2c_read_reg(ADDR_BME688, 0x1D, data, 8) != 0)
        return -1;

    uint32_t press_raw = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4);
    uint32_t temp_raw = (data[3] << 12) | (data[4] << 4) | (data[5] >> 4);
    uint32_t hum_raw  = (data[6] << 8) | data[7];

    /* Simplified conversion (real implementation uses Bosch compensation formulas) */
    int16_t temp_c = (int16_t)((temp_raw >> 4) / 100.0f);
    uint32_t pressure_hpa = press_raw / 256.0f / 100.0f;
    uint32_t humidity_pct = (hum_raw / 1000.0f) * 100;  /* simplified */

    out->temperature_c = (float)temp_c;
    out->pressure_hpa = (float)pressure_hpa;
    out->humidity_pct = (float)humidity_pct;
    out->voc_index    = 100;  /* simplified; real IAQ needs Bosch BSEC library */
    out->gas_resistance_ohm = 0;  /* requires gas heater config */

    ESP_LOGD(TAG, "BME688: T=%.1f°C P=%.0f hPa H=%.0f%%",
             out->temperature_c, out->pressure_hpa, out->humidity_pct);
    return 0;
}

/* ── SGP40 ─────────────────────────────────────────────── */
int sgp40_init(void)
{
    /* SGP40 self-test */
    uint8_t cmd[2] = { 0x28, 0x0E };  /* MeasureTest */
    uint8_t result[3];
    if (i2c_write(ADDR_SGP40, cmd, 2) != 0)
        return -1;
    vTaskDelay(pdMS_TO_TICKS(250));
    if (i2c_read(ADDR_SGP40, result, 3) != 0)
        return -1;

    ESP_LOGI(TAG, "SGP40 initialized (self-test result: 0x%02X%02X)",
             result[0], result[1]);
    return 0;
}

int sgp40_read(uint16_t *voc_index)
{
    /* MeasureRaw: command 0x26, 0x0F
       Takes 2 bytes: default RH=0x8000, T=0x6666 (25°C, 50% RH) */
    uint8_t cmd[4] = { 0x26, 0x0F, 0x80, 0x00 };
    /* CRC for conditioning bytes (simplified) */
    cmd[3] = 0x00;  /* would compute CRC-8 */

    if (i2c_write(ADDR_SGP40, cmd, 4) != 0)
        return -1;
    vTaskDelay(pdMS_TO_TICKS(250));

    uint8_t result[3];
    if (i2c_read(ADDR_SGP40, result, 3) != 0)
        return -1;

    /* Convert raw signal to VOC index (0-500) via Sensirion algorithm */
    uint16_t raw = (result[0] << 8) | result[1];
    /* Simplified: map raw (0-65535) to VOC index (0-500) */
    *voc_index = (raw > 0) ? (500 - (raw / 131)) : 500;
    if (*voc_index > 500) *voc_index = 500;

    ESP_LOGD(TAG, "SGP40: VOC index=%u (raw=0x%04X)", *voc_index, raw);
    return 0;
}

/* ── SCD41 ─────────────────────────────────────────────── */
int scd41_init(void)
{
    /* Start periodic measurement */
    uint8_t cmd[2] = { 0x21, 0xB1 };  /* StartPeriodicMeasurement */
    int ret = i2c_write(ADDR_SCD41, cmd, 2);
    if (ret != 0) return -1;

    ESP_LOGI(TAG, "SCD41 initialized — waiting for first measurement (5s)...");
    vTaskDelay(pdMS_TO_TICKS(SCD41_WARMUP_MS));
    return 0;
}

int scd41_read(scd41_data_t *out)
{
    /* Read measurement: command 0xEC, 0x05 → 9 bytes (CO2, T, RH, each with CRC) */
    uint8_t cmd[2] = { 0xEC, 0x05 };
    if (i2c_write(ADDR_SCD41, cmd, 2) != 0)
        return -1;

    vTaskDelay(pdMS_TO_TICKS(5));  /* wait for measurement */

    uint8_t data[9];
    if (i2c_read(ADDR_SCD41, data, 9) != 0)
        return -1;

    /* CO2: bytes 0-1 + CRC byte 2 */
    out->co2_ppm = (data[0] << 8) | data[1];
    /* Temperature: bytes 3-4 + CRC byte 5 → 175 * raw / 2^16 - 45 */
    uint16_t temp_raw = (data[3] << 8) | data[4];
    out->temperature_c = 175.0f * temp_raw / 65536.0f - 45.0f;
    /* Humidity: bytes 6-7 + CRC byte 8 → 100 * raw / 2^16 */
    uint16_t rh_raw = (data[6] << 8) | data[7];
    out->humidity_pct = 100.0f * rh_raw / 65536.0f;

    ESP_LOGD(TAG, "SCD41: CO2=%u ppm, T=%.1f°C, RH=%.1f%%",
             out->co2_ppm, out->temperature_c, out->humidity_pct);
    return 0;
}

/* ── Pack into protocol struct ─────────────────────────── */
int sensors_pack_air_quality(air_quality_t *out)
{
    pmsa_data_t pm;
    bme688_data_t bme;
    uint16_t voc;
    scd41_data_t co2;

    if (pmsa003i_read(&pm) != 0) return -1;
    if (bme688_read(&bme) != 0) return -2;
    if (sgp40_read(&voc) != 0) return -3;
    if (scd41_read(&co2) != 0) return -4;

    out->pm1_0     = pm.pm1_0_ug * 10;
    out->pm2_5    = pm.pm2_5_ug * 10;
    out->pm10     = pm.pm10_ug * 10;
    out->voc_index = bme.voc_index;
    out->hcho_ppb = voc;  /* SGP40 VOC index as HCHO proxy */
    out->co2_ppm  = co2.co2_ppm;
    out->temp_c_x10 = (int16_t)(bme.temperature_c * 10);
    out->rh_x10    = (uint16_t)(bme.humidity_pct * 10);

    ESP_LOGI(TAG, "Air quality: PM2.5=%.1f µg/m³, CO2=%u ppm, VOC=%u",
             out->pm2_5 / 10.0f, out->co2_ppm, out->voc_index);
    return 0;
}