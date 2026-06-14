/**
 * BreathHome - Wearable Breath Tag Firmware
 * nRF52832 + SGP30 + SHT40 + LIS2DH12
 * 
 * Personal exposure monitor. Clips to shirt collar, tracks
 * what YOU are breathing. Vibration alerts when entering
 * poor air zones. Symptom button for asthma correlation.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

LOG_MODULE_REGISTER(breath_tag, LOG_LEVEL_INF);

/* ========== CONSTANTS ========== */
#define BLE_ADV_INTERVAL_MS       1000   /* 1 second advertising */
#define SENSOR_READ_INTERVAL_MS   10000  /* 10 seconds */
#define ACTIVITY_SAMPLE_HZ        13     /* LIS2DH at 12.5 Hz */
#define VIBRATE_DURATION_MS       200    /* 200ms vibration pulse */
#define BATTERY_LOW_PCT           15     /* % threshold for low batt alert */

#define I2C_SGP30_ADDR    0x58
#define I2C_SHT40_ADDR    0x44
#define I2C_LIS2DH12_ADDR 0x18

/* ========== DATA STRUCTURES ========== */

typedef struct {
    float eco2;        /* Equivalent CO2 in ppm (SGP30) */
    float tvoc;        /* Total VOC in ppb (SGP30) */
    float temperature; /* From SHT40 */
    float humidity;    /* From SHT40 */
    uint8_t activity;  /* 0=still, 1=walking, 2=running, 3=sleeping */
    uint8_t battery_pct;
    float personal_aqi; /* Computed from eco2 + tvoc */
} tag_data_t;

typedef struct {
    uint8_t tag_id;
    uint8_t symptom_flag;   /* 0=none, 1=wheeze, 2=cough, 3=SOBOE, 4=throat */
    uint8_t vibrate_mode;   /* 0=off, 1=short, 2=long, 3=pattern */
    uint8_t led_mode;      /* 0=off, 1=aqi_color, 2=always_green, 3=blink_alert */
    uint16_t alert_threshold_aqi;  /* Vibrate when AQI > this */
} tag_config_t;

/* ========== GLOBALS ========== */
static tag_data_t tag;
static tag_config_t config = {
    .tag_id = 0,
    .symptom_flag = 0,
    .vibrate_mode = 1,
    .led_mode = 1,
    .alert_threshold_aqi = 100,
};

static uint8_t ble_aqi_data[4];  /* AQI, eCO2/10, TVOC/10, activity */
static uint16_t ble_batt_data;
static uint8_t ble_activity_data;
static uint8_t ble_symptom_data = 0;

/* GPIO definitions */
#define VIBRATE_PIN    12
#define LED_DATA_PIN    13
#define BTN_PIN         11
#define CHG_STATUS_PIN  14
#define VBAT_SENSE_PIN  15

static const struct device *i2c_dev;
static const struct device *adc_dev;

/* ========== SGP30 (eCO2 + TVOC) ========== */

/**
 * sgp30_init - Initialize Sensirion SGP30
 */
static int sgp30_init(void)
{
    /* Init: 0x20, 0x03 */
    uint8_t init_cmd[2] = {0x20, 0x03};
    int ret = i2c_write(i2c_dev, init_cmd, 2, I2C_SGP30_ADDR);
    if (ret != 0) {
        LOG_ERR("SGP30 init failed: %d", ret);
        return ret;
    }
    k_msleep(2);  /* 2ms startup */
    
    /* Set baseline: start measurement in ultra-low-power mode */
    uint8_t measure_cmd[2] = {0x20, 0x61};  /* Measure raw signals (ultra-low-power) */
    ret = i2c_write(i2c_dev, measure_cmd, 2, I2C_SGP30_ADDR);
    
    return ret;
}

/**
 * sgp30_read - Read eCO2 and TVOC from SGP30
 */
static int sgp30_read(float *eco2, float *tvoc)
{
    /* Measure in ultra-low-power mode */
    uint8_t measure_cmd[2] = {0x20, 0x61};
    int ret = i2c_write(i2c_dev, measure_cmd, 2, I2C_SGP30_ADDR);
    if (ret != 0) return ret;
    
    k_msleep(50);  /* Measurement takes ~50ms */
    
    uint8_t data[6];
    ret = i2c_read(i2c_dev, data, 6, I2C_SGP30_ADDR);
    if (ret != 0) return ret;
    
    /* eCO2: first 2 words (4 bytes), TVOC: last 2 bytes */
    /* In ultra-low-power mode, data is raw H2 and Ethanol signals */
    /* For eCO2/TVOC, use standard measurement mode */
    
    /* Use standard measurement for eCO2/TVOC */
    measure_cmd[0] = 0x20;
    measure_cmd[1] = 0x08;  /* Measure IAQ */
    ret = i2c_write(i2c_dev, measure_cmd, 2, I2C_SGP30_ADDR);
    if (ret != 0) return ret;
    
    k_msleep(50);
    ret = i2c_read(i2c_dev, data, 6, I2C_SGP30_ADDR);
    if (ret != 0) return ret;
    
    /* CRC check omitted for brevity */
    uint16_t eco2_raw = (data[0] << 8) | data[1];
    uint16_t tvoc_raw = (data[3] << 8) | data[4];
    
    *eco2 = (float)eco2_raw;
    *tvoc = (float)tvoc_raw;
    
    return 0;
}

/**
 * sgp30_set_humidity_compensation - Set humidity for SGP30 accuracy
 */
static void sgp30_set_humidity(float humidity)
{
    /* SGP30 can use absolute humidity for better accuracy */
    /* Calculate absolute humidity from RH and temperature */
    float ah = 216.7f * (6.112f * expf((17.62f * 22.0f) / (243.12f + 22.0f)) * 
               (humidity / 100.0f) / (273.15f + 22.0f));
    
    uint16_t ah_raw = (uint16_t)(ah * 100.0f);  /* In g/m³ * 100 */
    
    uint8_t cmd[4] = {
        0x20, 0x61,  /* Set absolute humidity */
        (ah_raw >> 8) & 0xFF,
        ah_raw & 0xFF
    };
    
    i2c_write(i2c_dev, cmd, 4, I2C_SGP30_ADDR);
}

/* ========== SHT40 (Temperature + Humidity) ========== */

/**
 * sht40_read - Read temperature and humidity from SHT40
 */
static int sht40_read(float *temperature, float *humidity)
{
    /* Start measurement: high precision mode */
    uint8_t cmd[1] = {0xFD};  /* Start measurement, high repeatability */
    int ret = i2c_write(i2c_dev, cmd, 1, I2C_SHT40_ADDR);
    if (ret != 0) return ret;
    
    k_msleep(10);  /* Measurement takes ~8ms */
    
    uint8_t data[6];
    ret = i2c_read(i2c_dev, data, 6, I2C_SHT40_ADDR);
    if (ret != 0) return ret;
    
    /* Temperature: bytes 0-1 (with CRC byte 2) */
    uint16_t temp_raw = (data[0] << 8) | data[1];
    *temperature = -45.0f + 175.0f * (float)temp_raw / 65535.0f;
    
    /* Humidity: bytes 3-4 (with CRC byte 5) */
    uint16_t rh_raw = (data[3] << 8) | data[4];
    *humidity = -6.0f + 125.0f * (float)rh_raw / 65535.0f;
    
    /* Clamp */
    if (*humidity < 0.0f) *humidity = 0.0f;
    if (*humidity > 100.0f) *humidity = 100.0f;
    
    return 0;
}

/* ========== LIS2DH12 (Accelerometer) ========== */

/**
 * lis2dh12_init - Initialize accelerometer
 */
static int lis2dh12_init(void)
{
    /* Set to normal mode, 12.5 Hz, all axes enabled */
    uint8_t ctrl1[2] = {0x20, 0x27};  /* CTRL_REG1: 12.5 Hz, normal, XYZ */
    int ret = i2c_write(i2c_dev, ctrl1, 2, I2C_LIS2DH12_ADDR);
    if (ret != 0) return ret;
    
    /* Set range to ±2g, high resolution */
    uint8_t ctrl4[2] = {0x23, 0x08};  /* CTRL_REG4: ±2g, HR */
    ret = i2c_write(i2c_dev, ctrl4, 2, I2C_LIS2DH12_ADDR);
    
    /* Enable click detection on INT1 */
    uint8_t click_cfg[2] = {0x38, 0x15};  /* CLICK_CFG: single XYZ */
    i2c_write(i2c_dev, click_cfg, 2, I2C_LIS2DH12_ADDR);
    
    uint8_t click_ths[2] = {0x3A, 0x08};  /* CLICK_THS: threshold */
    i2c_write(i2c_dev, click_ths, 2, I2C_LIS2DH12_ADDR);
    
    uint8_t time_limit[2] = {0x3B, 0x10};  /* TIME_LIMIT */
    i2c_write(i2c_dev, time_limit, 2, I2C_LIS2DH12_ADDR);
    
    return ret;
}

/**
 * lis2dh12_read_accel - Read 3-axis acceleration
 */
static int lis2dh12_read_accel(float *x, float *y, float *z)
{
    uint8_t reg = 0x28 | 0x80;  /* OUT_X_L, auto-increment */
    int ret = i2c_write(i2c_dev, &reg, 1, I2C_LIS2DH12_ADDR);
    if (ret != 0) return ret;
    
    uint8_t data[6];
    ret = i2c_read(i2c_dev, data, 6, I2C_LIS2DH12_ADDR);
    if (ret != 0) return ret;
    
    /* 16-bit signed, HR mode: ±2g range, 1mg/LSB */
    int16_t raw_x = (int16_t)((data[1] << 8) | data[0]) >> 4;
    int16_t raw_y = (int16_t)((data[3] << 8) | data[2]) >> 4;
    int16_t raw_z = (int16_t)((data[5] << 8) | data[4]) >> 4;
    
    /* Convert to g (HR mode: 1mg/LSB, but right-shifted by 4) */
    *x = (float)raw_x * 0.001f;
    *y = (float)raw_y * 0.001f;
    *z = (float)raw_z * 0.001f;
    
    return 0;
}

/**
 * classify_activity - Simple activity classification from accelerometer
 * Uses magnitude variance and mean to classify:
 * - Still: low variance, low mean
 * - Walking: moderate variance, moderate mean
 * - Running: high variance, high mean
 * - Sleeping: very low variance, ~1g on Z axis
 */
static uint8_t classify_activity(float x, float y, float z)
{
    float magnitude = sqrtf(x*x + y*y + z*z);
    
    /* Simplified: use magnitude only */
    /* In production, use rolling window of 3-second samples */
    static float mag_history[32];
    static int mag_idx = 0;
    
    mag_history[mag_idx] = magnitude;
    mag_idx = (mag_idx + 1) % 32;
    
    /* Calculate variance */
    float mean = 0, variance = 0;
    for (int i = 0; i < 32; i++) {
        mean += mag_history[i];
    }
    mean /= 32.0f;
    
    for (int i = 0; i < 32; i++) {
        float diff = mag_history[i] - mean;
        variance += diff * diff;
    }
    variance /= 32.0f;
    
    if (variance < 0.001f && fabsf(mean - 1.0f) < 0.1f) {
        return 3;  /* Sleeping (still, ~1g on Z) */
    } else if (variance < 0.01f) {
        return 0;  /* Still */
    } else if (variance < 0.05f) {
        return 1;  /* Walking */
    } else {
        return 2;  /* Running */
    }
}

/* ========== BLE ADVERTISING ========== */

static struct bt_data ad_data[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0xEA, 0xBH),  /* BreathHome service UUID */
};

static uint8_t mfg_data[8] = {
    0xEA, 0xBH,  /* Company ID (BreathHome) */
    0x00, 0x00,  /* Tag ID */
    0x00,         /* Battery % */
    0x00,         /* AQI */
    0x00,         /* Activity */
    0x00,         /* Symptom flag */
};

static const struct bt_data scan_data[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, "BH-TAG-0000", 12),
};

/**
 * ble_update_advertising - Update BLE advertising data with current readings
 */
static void ble_update_advertising(void)
{
    mfg_data[2] = config.tag_id;
    mfg_data[3] = 0;
    mfg_data[4] = tag.battery_pct;
    mfg_data[5] = (uint8_t)(tag.personal_aqi > 255 ? 255 : tag.personal_aqi);
    mfg_data[6] = tag.activity;
    mfg_data[7] = config.symptom_flag;
    
    /* Update advertising data */
    struct bt_data updated_ad[] = {
        BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
        BT_DATA(BT_DATA_MANUFACTURER_DATA, mfg_data, sizeof(mfg_data)),
    };
    
    bt_le_adv_update_data(updated_ad, ARRAY_SIZE(updated_ad), scan_data, ARRAY_SIZE(scan_data));
}

/* ========== PERSONAL AQI CALCULATION ========== */

/**
 * calculate_personal_aqi - Compute personal AQI from eCO2 and TVOC
 * This is a simplified personal index, not the full room AQI
 */
static float calculate_personal_aqi(float eco2, float tvoc)
{
    /* eCO2 sub-index */
    float co2_idx;
    if (eco2 <= 800.0f) {
        co2_idx = 50.0f * eco2 / 800.0f;
    } else if (eco2 <= 1200.0f) {
        co2_idx = 50.0f + 50.0f * (eco2 - 800.0f) / 400.0f;
    } else if (eco2 <= 1800.0f) {
        co2_idx = 100.0f + 50.0f * (eco2 - 1200.0f) / 600.0f;
    } else {
        co2_idx = 150.0f + 50.0f * (eco2 - 1800.0f) / 700.0f;
    }
    
    /* TVOC sub-index */
    float voc_idx;
    if (tvoc <= 100.0f) {
        voc_idx = 50.0f * tvoc / 100.0f;
    } else if (tvoc <= 300.0f) {
        voc_idx = 50.0f + 100.0f * (tvoc - 100.0f) / 200.0f;
    } else {
        voc_idx = 150.0f + 100.0f * (tvoc - 300.0f) / 700.0f;
    }
    
    /* Max of sub-indices */
    float aqi = co2_idx > voc_idx ? co2_idx : voc_idx;
    
    if (aqi > 500.0f) aqi = 500.0f;
    if (aqi < 0.0f) aqi = 0.0f;
    
    return aqi;
}

/* ========== LED CONTROL ========== */

/**
 * ws2812b_set_color - Set RGB LED color (single WS2812B mini)
 * Uses bit-banging on GPIO (suitable for nRF52832)
 */
static void ws2812b_set_color(uint8_t r, uint8_t g, uint8_t b)
{
    /* WS2812B protocol: 800kHz, 24 bits (GRB) */
    /* 0 bit: 0.4µs high, 0.8µs low */
    /* 1 bit: 0.8µs high, 0.4µs low */
    
    uint8_t grb[3] = {g, r, b};  /* WS2812B is GRB order */
    
    /* Disable interrupts for precise timing */
    __disable_irq();
    
    for (int byte_idx = 0; byte_idx < 3; byte_idx++) {
        for (int bit = 7; bit >= 0; bit--) {
            if (grb[byte_idx] & (1 << bit)) {
                /* 1 bit: 0.8µs high, 0.4µs low */
                gpio_pin_set_raw(gpio_port, LED_DATA_PIN, 1);
                /* ~12 cycles at 64MHz = 0.8µs */
                for (volatile int i = 0; i < 12; i++) __NOP();
                gpio_pin_set_raw(gpio_port, LED_DATA_PIN, 0);
                for (volatile int i = 0; i < 6; i++) __NOP();
            } else {
                /* 0 bit: 0.4µs high, 0.8µs low */
                gpio_pin_set_raw(gpio_port, LED_DATA_PIN, 1);
                for (volatile int i = 0; i < 6; i++) __NOP();
                gpio_pin_set_raw(gpio_port, LED_DATA_PIN, 0);
                for (volatile int i = 0; i < 12; i++) __NOP();
            }
        }
    }
    
    /* Latch: >50µs low */
    gpio_pin_set_raw(gpio_port, LED_DATA_PIN, 0);
    __enable_irq();
    k_msleep(1);
}

/**
 * update_led_from_aqi - Set LED color based on personal AQI
 */
static void update_led_from_aqi(float aqi)
{
    if (config.led_mode == 0) {
        ws2812b_set_color(0, 0, 0);  /* Off */
        return;
    }
    
    if (config.led_mode == 2) {
        ws2812b_set_color(0, 128, 0);  /* Always green */
        return;
    }
    
    /* AQI-based coloring */
    if (aqi <= 50) {
        ws2812b_set_color(0, 128, 0);  /* Green: Good */
    } else if (aqi <= 100) {
        ws2812b_set_color(128, 128, 0);  /* Yellow: Moderate */
    } else if (aqi <= 150) {
        ws2812b_set_color(255, 128, 0);  /* Orange: Unhealthy for Sensitive */
    } else if (aqi <= 200) {
        ws2812b_set_color(255, 0, 0);  /* Red: Unhealthy */
    } else {
        ws2812b_set_color(128, 0, 128);  /* Purple: Very Unhealthy/Hazardous */
    }
}

/* ========== VIBRATION ALERT ========== */

/**
 * vibrate_alert - Trigger vibration motor for air quality alert
 */
static void vibrate_alert(uint8_t mode)
{
    switch (mode) {
        case 1:  /* Short pulse */
            gpio_pin_set_raw(gpio_port, VIBRATE_PIN, 1);
            k_msleep(VIBRATE_DURATION_MS);
            gpio_pin_set_raw(gpio_port, VIBRATE_PIN, 0);
            break;
        case 2:  /* Long pulse */
            gpio_pin_set_raw(gpio_port, VIBRATE_PIN, 1);
            k_msleep(500);
            gpio_pin_set_raw(gpio_port, VIBRATE_PIN, 0);
            break;
        case 3:  /* Pattern: 3 short pulses */
            for (int i = 0; i < 3; i++) {
                gpio_pin_set_raw(gpio_port, VIBRATE_PIN, 1);
                k_msleep(150);
                gpio_pin_set_raw(gpio_port, VIBRATE_PIN, 0);
                k_msleep(100);
            }
            break;
        default:
            break;
    }
}

/* ========== BATTERY MONITORING ========== */

/**
 * read_battery_pct - Read battery voltage and estimate percentage
 */
static uint8_t read_battery_pct(void)
{
    /* Read ADC on VBAT_SENSE pin (through voltage divider) */
    /* Lipo: 4.2V = 100%, 3.3V = 0% */
    /* Voltage divider: 2:1, so ADC reads 0-2.1V */
    
    int16_t adc_val;
    struct adc_sequence seq = {
        .channels = BIT(0),
        .buffer = &adc_val,
        .buffer_size = sizeof(adc_val),
    };
    adc_read(adc_dev, &seq);
    
    float voltage = (float)adc_val * 3.3f / 4096.0f * 2.0f;  /* *2 for divider */
    
    /* Lipo discharge curve (simplified) */
    float pct;
    if (voltage >= 4.2f) pct = 100.0f;
    else if (voltage >= 4.0f) pct = 80.0f + 20.0f * (voltage - 4.0f) / 0.2f;
    else if (voltage >= 3.7f) pct = 20.0f + 60.0f * (voltage - 3.7f) / 0.3f;
    else if (voltage >= 3.3f) pct = 5.0f + 15.0f * (voltage - 3.3f) / 0.4f;
    else pct = 5.0f * voltage / 3.3f;
    
    return (uint8_t)(pct > 100 ? 100 : pct);
}

/* ========== BUTTON HANDLING ========== */

/**
 * button_handler - Symptom button press handler
 * Short press: cycle symptom (wheeze → cough → SOBOE → throat → cancel)
 * Long press (3s): "I'm OK" cancel
 */
static void button_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    static int64_t press_time = 0;
    static uint8_t symptom_cycle = 0;
    
    int64_t now = k_uptime_get();
    
    if (gpio_pin_get_raw(gpio_port, BTN_PIN) == 0) {
        /* Button pressed (active low) */
        press_time = now;
    } else {
        /* Button released */
        int64_t duration = now - press_time;
        
        if (duration > 3000) {
            /* Long press: "I'm OK" cancel */
            config.symptom_flag = 0;
            ble_symptom_data = 0;
            ws2812b_set_color(0, 64, 0);  /* Solid green for 3 seconds */
            k_msleep(3000);
        } else if (duration > 50) {
            /* Short press: cycle through symptoms */
            symptom_cycle = (symptom_cycle + 1) % 5;
            config.symptom_flag = symptom_cycle;
            ble_symptom_data = symptom_cycle;
            
            /* Vibrate to confirm */
            vibrate_alert(1);  /* Short pulse */
        }
    }
}

static struct gpio_callback btn_cb;

/* ========== MAIN THREADS ========== */

static void sensor_thread(void *p1, void *p2, void *p3)
{
    /* Initialize sensors */
    sgp30_init();
    lis2dh12_init();
    
    /* SGP30 needs 20 seconds warmup for stable readings */
    /* During warmup, use default values */
    tag.eco2 = 400.0f;
    tag.tvoc = 0.0f;
    tag.personal_aqi = 0.0f;
    tag.activity = 0;
    tag.battery_pct = read_battery_pct();
    
    k_msleep(2000);  /* Initial settling */
    
    while (1) {
        /* Read SGP30 */
        float eco2, tvoc;
        if (sgp30_read(&eco2, &tvoc) == 0) {
            tag.eco2 = eco2;
            tag.tvoc = tvoc;
        }
        
        /* Read SHT40 */
        float temp, rh;
        if (sht40_read(&temp, &rh) == 0) {
            tag.temperature = temp;
            tag.humidity = rh;
            /* Update SGP30 humidity compensation */
            sgp30_set_humidity_compensation(rh);
        }
        
        /* Read accelerometer */
        float ax, ay, az;
        if (lis2dh12_read_accel(&ax, &ay, &az) == 0) {
            tag.activity = classify_activity(ax, ay, az);
        }
        
        /* Calculate personal AQI */
        tag.personal_aqi = calculate_personal_aqi(tag.eco2, tag.tvoc);
        
        /* Check alert thresholds */
        if (tag.personal_aqi > config.alert_threshold_aqi) {
            vibrate_alert(config.vibrate_mode);
            update_led_from_aqi(tag.personal_aqi);
        } else {
            update_led_from_aqi(tag.personal_aqi);
        }
        
        /* Update battery */
        tag.battery_pct = read_battery_pct();
        
        /* Update BLE advertising data */
        ble_update_advertising();
        
        /* Update BLE GATT characteristics */
        ble_aqi_data[0] = (uint8_t)(tag.personal_aqi > 255 ? 255 : tag.personal_aqi);
        ble_aqi_data[1] = (uint8_t)(tag.eco2 / 10);
        ble_aqi_data[2] = (uint8_t)(tag.tvoc > 255 ? 255 : tag.tvoc);
        ble_aqi_data[3] = tag.activity;
        ble_batt_data = tag.battery_pct;
        ble_activity_data = tag.activity;
        
        k_msleep(SENSOR_READ_INTERVAL_MS);
    }
}

K_THREAD_DEFINE(sensor_tid, 1024, sensor_thread, NULL, NULL, NULL, 5, 0, 0);

int main(void)
{
    /* Initialize I2C */
    i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
    
    /* Initialize ADC */
    adc_dev = DEVICE_DT_GET(DT_NODELABEL(adc1));
    
    /* Initialize GPIOs */
    gpio_pin_configure(gpio_port, VIBRATE_PIN, GPIO_OUTPUT_LOW);
    gpio_pin_configure(gpio_port, LED_DATA_PIN, GPIO_OUTPUT_LOW);
    gpio_pin_configure(gpio_port, BTN_PIN, GPIO_INPUT | GPIO_PULL_UP);
    
    /* Button interrupt */
    gpio_pin_interrupt_configure(gpio_port, BTN_PIN, GPIO_INT_EDGE_BOTH);
    gpio_init_callback(&btn_cb, button_handler, BIT(BTN_PIN));
    gpio_add_callback(gpio_port, &btn_cb);
    
    /* Initialize BLE */
    bt_enable(NULL);
    
    /* Start advertising */
    struct bt_le_adv_param adv_param = BT_LE_ADV_PARAM_INIT(
        BT_LE_ADV_OPT_USE_IDENTITY,
        BT_GAP_ADV_INTERVAL(1000),  /* 1 second */
        BT_GAP_ADV_INTERVAL(1000),
        NULL
    );
    
    bt_le_adv_start(&adv_param, ad_data, ARRAY_SIZE(ad_data), scan_data, ARRAY_SIZE(scan_data));
    
    /* Set initial LED state */
    ws2812b_set_color(0, 64, 0);  /* Green during initialization */
    
    printk("BreathHome Wearable Tag started\n");
    printk("Tag ID: %d\n", config.tag_id);
    printk("BLE advertising active\n");
    
    /* Main thread handles button presses and LED updates */
    /* Sensor thread runs in background */
    
    while (1) {
        k_msleep(1000);
    }
    
    return 0;
}