/**
 * SightSync Desk Sentinel — Sensor Drivers Implementation
 *
 * VL53L1X (ToF distance), VEML7700 (ambient lux),
 * TCS34725 (RGBC color), APDS9306 (blue light).
 *
 * License: MIT
 */

#include "sensors.h"
#include "esp_log.h"
#include "driver/i2c.h"

static const char *TAG = "sensors";

/* I²C addresses */
#define VL53L1X_ADDR   0x29
#define VEML7700_ADDR  0x10
#define TCS34725_ADDR 0x29  /* Note: same as VL53L1X if both present — use mux in production */
#define APDS9306_ADDR  0x0C  /* APDS-9306 digital ambient light sensor */

/* TCS34725 and VL53L1X share address 0x29 — in production use TCA9548A I²C mux.
 * For this reference, we assume they're on separate I²C buses or time-multiplexed.
 */

static uint8_t i2c_read(uint8_t addr, uint8_t reg)
{
    uint8_t val;
    i2c_master_write_read_device(I2C_NUM_0, addr, &reg, 1, &val, 1, 100);
    return val;
}

static void i2c_write(uint8_t addr, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    i2c_master_write_to_device(I2C_NUM_0, addr, buf, 2, 100);
}

static uint16_t i2c_read16(uint8_t addr, uint8_t reg)
{
    uint8_t buf[2];
    i2c_master_write_read_device(I2C_NUM_0, addr, &reg, 1, buf, 2, 100);
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

static void i2c_write16(uint8_t addr, uint8_t reg, uint16_t val)
{
    uint8_t buf[3] = {reg, val & 0xFF, (val >> 8) & 0xFF};
    i2c_master_write_to_device(I2C_NUM_0, addr, buf, 3, 100);
}

/* ── VL53L1X ToF Distance Sensor ──────────────────────────────────── */

void sensors_init(void)
{
    /* VL53L1X: software reset */
    i2c_write(VL53L1X_ADDR, 0x0001, 0x00);  /* SOFT_RESET */
    vTaskDelay(pdMS_TO_TICKS(100));

    /* VL53L1X: configure for short distance mode */
    i2c_write16(VL53L1X_ADDR, 0x002E, 0x000B);  /* ALGO__PART_TO_PART_RANGE_OFFSET_MM */
    i2c_write16(VL53L1X_ADDR, 0x0030, 0x0000);  /* RANGE_CONFIG_THRESHOLD */

    ESP_LOGI(TAG, "VL53L1X initialized");

    /* VEML7700: configure continuous mode, 100ms integration */
    i2c_write16(VEML7700_ADDR, 0x00, 0x0080);  /* ALS_CONF: power on, 100ms */
    ESP_LOGI(TAG, "VEML7700 initialized");

    /* TCS34725: enable, 2.4ms integration time, 1× gain */
    i2c_write(TCS34725_ADDR, 0x00, 0x01);  /* ENABLE → PON */
    vTaskDelay(pdMS_TO_TICKS(3));
    i2c_write(TCS34725_ADDR, 0x00, 0x03);  /* ENABLE → PON + AEN */
    i2c_write(TCS34725_ADDR, 0x01, 0x00);  /* ATIME → 2.4ms */
    i2c_write(TCS34725_ADDR, 0x0D, 0x00);  /* AGAIN → 1× */
    ESP_LOGI(TAG, "TCS34725 initialized");

    /* APDS9306: power on, continuous */
    i2c_write(APDS9306_ADDR, 0x00, 0x01);  /* Main Control → Power On */
    ESP_LOGI(TAG, "APDS9306 initialized");
}

uint16_t sensors_read_distance(uint8_t *quality)
{
    /* VL53L1X: check range status */
    uint8_t status = i2c_read(VL53L1X_ADDR, 0x0089);  /* RESULT__RANGE_STATUS */
    if (status & 0x01) {
        /* Data ready */
        uint16_t range = i2c_read16(VL53L1X_ADDR, 0x0096);  /* RESULT__RANGE_MM */
        uint8_t error = (status >> 2) & 0x0F;
        if (error == 0) {
            *quality = 3;  /* high */
        } else if (error < 5) {
            *quality = 2;  /* medium */
        } else {
            *quality = 1;  /* low */
            return range;  /* still return range, but flag lower quality */
        }
        /* Clear interrupt */
        i2c_write(VL53L1X_ADDR, 0x0015, 0x01);  /* SYSTEM__INTERRUPT_CLEAR */
        return range;
    }
    *quality = 0;  /* invalid */
    return 0;
}

uint16_t sensors_read_lux(void)
{
    /* VEML7700: read ALS result register */
    uint16_t raw = i2c_read16(VEML7700_ADDR, 0x04);  /* ALS result */
    /* Convert to lux: factor depends on integration time & gain.
     * For 100ms, 1× gain: lux = raw × 0.0576
     * Simplified: lux = raw × 576 / 10000
     */
    uint32_t lux = (uint32_t)raw * 576 / 10000;
    if (lux > 65535) lux = 65535;
    return (uint16_t)lux;
}

void sensors_read_rgbc(uint16_t *r, uint16_t *g, uint16_t *b, uint16_t *c)
{
    *c = i2c_read16(TCS34725_ADDR, 0x14);  /* CDATA */
    *r = i2c_read16(TCS34725_ADDR, 0x16);  /* RDATA */
    *g = i2c_read16(TCS34725_ADDR, 0x18);  /* GDATA */
    *b = i2c_read16(TCS34725_ADDR, 0x1A);  /* BDATA */
}

uint16_t sensors_estimate_cct(uint16_t r, uint16_t g, uint16_t b, uint16_t c)
{
    (void)c;
    if (g == 0) return 0;
    /* McCamy's formula approximation:
     * CCT = 449*n³ + 3525*n² + 6823.3*n + 5520.33
     * where n = (x - 0.3320) / (0.1858 - y)
     * x = X / (X+Y+Z), y = Y / (X+Y+Z)
     * X = 1.127*R - 0.510*G - 0.319*B (simplified RGB→XYZ)
     * Y = 0.413*R + 0.599*G - 0.057*B
     * Z = 0.057*R - 0.018*G + 1.256*B
     */
    float X = 1.127f * r - 0.510f * g - 0.319f * b;
    float Y = 0.413f * r + 0.599f * g - 0.057f * b;
    float Z = 0.057f * r - 0.018f * g + 1.256f * b;

    float sum = X + Y + Z;
    if (sum == 0) return 6500;  /* default */

    float x = X / sum;
    float y = Y / sum;

    float n = (x - 0.3320f) / (0.1858f - y);
    float cct = 449.0f * n*n*n + 3525.0f * n*n + 6823.3f * n + 5520.33f;

    if (cct < 1800) cct = 1800;
    if (cct > 12000) cct = 12000;
    return (uint16_t)cct;
}

uint16_t sensors_read_blue_light(void)
{
    /* APDS9306: read channel 0 (visible + IR) and channel 1 (IR only).
     * Blue light ≈ ch0 - ch1 (visible without IR component),
     * weighted by 470 nm filter response.
     */
    uint16_t ch0 = i2c_read16(APDS9306_ADDR, 0x0C);  /* CH0 DATA */
    uint16_t ch1 = i2c_read16(APDS9306_ADDR, 0x0E);  /* CH1 DATA */

    if (ch0 > ch1) {
        return (ch0 - ch1) * 10;  /* ×10 to get mW/m² scale */
    }
    return 0;
}