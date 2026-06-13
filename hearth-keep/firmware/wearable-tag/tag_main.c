/**
 * HearthKeep Wearable Tag Firmware
 * 
 * MCU: nRF52810 (Ultra-low-power BLE 5.0)
 * Accelerometer: LIS2DH12
 * Button: 12mm tactile (panic)
 * Buzzer: 12mm piezo
 * LED: WS2812B mini RGB
 * Power: CR2032 coin cell
 * 
 * Responsibilities:
 * - BLE 5.0 connectionless advertising (presence beacon)
 * - Panic button: immediate BLE alert to hub
 * - Accelerometer: always-on 25Hz fall detection
 * - Long-press (3s) for "I'm OK" cancel
 * - Battery monitoring and low-battery alerts
 * - 6+ month battery life on CR2032
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>
#include <string.h>

#include "../common/mesh_protocol.h"

LOG_MODULE_REGISTER(hk_tag, LOG_LEVEL_INF);

/* ========================================================================
 * HARDWARE DEFINITIONS
 * ======================================================================== */

/* Panic button */
#define PANIC_BTN_NODE DT_ALIAS(sw0)
static const struct gpio_dt_spec panic_btn = GPIO_DT_SPEC_GET(PANIC_BTN_NODE, gpios);
static struct gpio_callback panic_btn_cb;

/* Status LED */
#define LED_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led_data = GPIO_DT_SPEC_GET(LED_NODE, gpios);

/* Piezo buzzer */
#define PIEZO_NODE DT_ALIAS(piezo0)
static const struct gpio_dt_spec piezo = GPIO_DT_SPEC_GET(PIEZO_NODE, gpios);

/* LIS2DH12 I2C */
#define ACCEL_I2C_DEV DT_NODELABEL(i2c0)
#define LIS2DH12_ADDR 0x19

/* Battery ADC */
#define VBAT_SENSE_DT DT_ALIAS(vbat_sense)

/* ========================================================================
 * LIS2DH12 ACCELEROMETER DRIVER
 * ======================================================================== */

#define LIS2DH12_REG_WHO_AM_I    0x0F
#define LIS2DH12_REG_CTRL1        0x20
#define LIS2DH12_REG_CTRL2        0x21
#define LIS2DH12_REG_CTRL3        0x22
#define LIS2DH12_REG_CTRL4        0x23
#define LIS2DH12_REG_CTRL5        0x24
#define LIS2DH12_REG_CTRL6        0x25
#define LIS2DH12_REG_INT1_CFG     0x30
#define LIS2DH12_REG_INT1_SRC     0x31
#define LIS2DH12_REG_INT1_THS     0x32
#define LIS2DH12_REG_INT1_DUR     0x33
#define LIS2DH12_REG_INT2_CFG     0x34
#define LIS2DH12_REG_OUT_X_L      0x28
#define LIS2DH12_REG_OUT_X_H      0x29
#define LIS2DH12_REG_OUT_Y_L      0x2A
#define LIS2DH12_REG_OUT_Y_H      0x2B
#define LIS2DH12_REG_OUT_Z_L      0x2C
#define LIS2DH12_REG_OUT_Z_H      0x2D

typedef struct {
    float x;
    float y;
    float z;
} accel_data_t;

static int lis2dh12_init(const struct device *i2c)
{
    LOG_INF("Initializing LIS2DH12 accelerometer...");
    
    /* Check WHO_AM_I */
    uint8_t who_am_i;
    int ret = i2c_reg_read_byte(i2c, LIS2DH12_ADDR, LIS2DH12_REG_WHO_AM_I, &who_am_i);
    if (ret != 0 || who_am_i != 0x33) {
        LOG_ERR("LIS2DH12 not found (WHO_AM_I=0x%02X)", who_am_i);
        return -1;
    }
    
    /* CTRL1: 25Hz, low-power mode, all axes enabled */
    ret = i2c_reg_write_byte(i2c, LIS2DH12_ADDR, LIS2DH12_REG_CTRL1, 0x27);
    /* 0x27 = 25Hz | Normal mode | XYZ enabled */
    
    /* CTRL2: High-pass filter for interrupt */
    ret |= i2c_reg_write_byte(i2c, LIS2DH12_ADDR, LIS2DH12_REG_CTRL2, 0x01);
    
    /* CTRL3: INT1 on click, DRDY on INT2 */
    ret |= i2c_reg_write_byte(i2c, LIS2DH12_ADDR, LIS2DH12_REG_CTRL3, 0x40);
    
    /* CTRL4: ±2g full scale, high resolution */
    ret |= i2c_reg_write_byte(i2c, LIS2DH12_ADDR, LIS2DH12_REG_CTRL4, 0x08);
    
    /* Configure free-fall detection on INT1 */
    /* INT1_CFG: OR combination of X,Y,Z low events */
    ret |= i2c_reg_write_byte(i2c, LIS2DH12_ADDR, LIS2DH12_REG_INT1_CFG, 0x95);
    /* 0x95 = X-low OR Y-low OR Z-low | LIR (latch interrupt) */
    
    /* INT1_THS: Threshold = 350mg (7 LSB = 2g/128 * 7 ≈ 350mg) */
    ret |= i2c_reg_write_byte(i2c, LIS2DH12_ADDR, LIS2DH12_REG_INT1_THS, 0x16);
    
    /* INT1_DUR: Duration = 3/25Hz = 120ms */
    ret |= i2c_reg_write_byte(i2c, LIS2DH12_ADDR, LIS2DH12_REG_INT1_DUR, 0x03);
    
    /* CTRL6: INT2 on movement detection (click) */
    ret |= i2c_reg_write_byte(i2c, LIS2DH12_ADDR, LIS2DH12_REG_CTRL6, 0x00);
    
    if (ret != 0) {
        LOG_ERR("Failed to configure LIS2DH12");
        return ret;
    }
    
    LOG_INF("LIS2DH12 initialized: 25Hz, ±2g, free-fall detection enabled");
    return 0;
}

static int lis2dh12_read(const struct device *i2c, accel_data_t *data)
{
    uint8_t x_l, x_h, y_l, y_h, z_l, z_h;
    int ret;
    
    ret = i2c_reg_read_byte(i2c, LIS2DH12_ADDR, LIS2DH12_REG_OUT_X_L, &x_l);
    ret |= i2c_reg_read_byte(i2c, LIS2DH12_ADDR, LIS2DH12_REG_OUT_X_H, &x_h);
    ret |= i2c_reg_read_byte(i2c, LIS2DH12_ADDR, LIS2DH12_REG_OUT_Y_L, &y_l);
    ret |= i2c_reg_read_byte(i2c, LIS2DH12_ADDR, LIS2DH12_REG_OUT_Y_H, &y_h);
    ret |= i2c_reg_read_byte(i2c, LIS2DH12_ADDR, LIS2DH12_REG_OUT_Z_L, &z_l);
    ret |= i2c_reg_read_byte(i2c, LIS2DH12_ADDR, LIS2DH12_REG_OUT_Z_H, &z_h);
    
    if (ret != 0) return ret;
    
    /* Convert to g (±2g range, 16-bit, high resolution = 12-bit) */
    int16_t raw_x = (int16_t)((x_h << 8) | x_l) >> 4;
    int16_t raw_y = (int16_t)((y_h << 8) | y_l) >> 4;
    int16_t raw_z = (int16_t)((z_h << 8) | z_l) >> 4;
    
    /* 12-bit high resolution: 1 mg/digit at ±2g */
    data->x = raw_x * 0.001f;
    data->y = raw_y * 0.001f;
    data->z = raw_z * 0.001f;
    
    return 0;
}

static bool lis2dh12_check_freefall(const struct device *i2c)
{
    uint8_t int1_src;
    int ret = i2c_reg_read_byte(i2c, LIS2DH12_ADDR, LIS2DH12_REG_INT1_SRC, &int1_src);
    if (ret != 0) return false;
    
    /* Check if all axes are below threshold (free-fall) */
    return (int1_src & 0x95) == 0x95;  /* X-low + Y-low + Z-low */
}

/* ========================================================================
 * APPLICATION STATE
 * ======================================================================== */

typedef enum {
    TAG_STATE_NORMAL,
    TAG_STATE_PANIC,
    TAG_STATE_FALL_DETECTED,
    TAG_STATE_CANCEL,
    TAG_STATE_LOW_BATTERY,
} tag_state_t;

typedef struct {
    uint8_t tag_id;
    tag_state_t state;
    uint32_t uptime_ms;
    uint8_t battery_pct;
    bool panic_active;
    bool fall_detected;
    uint32_t panic_start_ms;
    uint32_t fall_start_ms;
    accel_data_t last_accel;
    float accel_magnitude_history[8];
    int accel_history_idx;
} tag_state_t;

static tag_state_t g_tag;

/* BLE advertising data */
static uint8_t mfg_data[] = {
    0x48, 0x4B,  /* Company ID: "HK" */
    0x00,         /* Tag ID (set at init) */
    0x64,         /* Battery % */
    0x00,         /* Panic status */
    0x00,         /* Fall status */
};

static const struct bt_data ad_data[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, "HK-TAG", sizeof("HK-TAG") - 1),
    BT_DATA(BT_DATA_MANUFACTURER_DATA, mfg_data, sizeof(mfg_data)),
};

/* ========================================================================
 * PIEZO BUZZER
 * ======================================================================== */

static void beep(uint16_t duration_ms, uint16_t frequency_hz)
{
    /* Generate PWM tone on piezo buzzer */
    uint32_t period_us = 1000000 / frequency_hz;
    uint32_t on_time_us = period_us / 2;
    uint32_t count = (duration_ms * 1000) / period_us;
    
    for (uint32_t i = 0; i < count; i++) {
        gpio_pin_set_dt(&piezo, 1);
        k_usleep(on_time_us);
        gpio_pin_set_dt(&piezo, 0);
        k_usleep(on_time_us);
    }
}

static void beep_panic(void)
{
    /* SOS pattern: 3 short, 3 long, 3 short */
    for (int i = 0; i < 3; i++) {
        beep(150, 2800);
        k_msleep(100);
    }
    k_msleep(300);
    for (int i = 0; i < 3; i++) {
        beep(400, 2800);
        k_msleep(100);
    }
    k_msleep(300);
    for (int i = 0; i < 3; i++) {
        beep(150, 2800);
        k_msleep(100);
    }
}

static void beep_ok(void)
{
    /* Two rising tones */
    beep(200, 2000);
    k_msleep(100);
    beep(300, 3000);
}

static void beep_low_battery(void)
{
    beep(500, 1000);
}

/* ========================================================================
 * WS2812B LED (Bit-bang)
 * ======================================================================== */

static void ws2812b_send_byte(uint8_t byte)
{
    /* WS2812B protocol: ~1.25µs per bit
     * 0-bit: 0.4µs high, 0.85µs low
     * 1-bit: 0.8µs high, 0.45µs low */
    for (int bit = 7; bit >= 0; bit--) {
        if (byte & (1 << bit)) {
            /* 1-bit */
            gpio_pin_set_dt(&led_data, 1);
            k_usleep(0);  /* ~0.8µs high (with overhead) */
            gpio_pin_set_dt(&led_data, 0);
            k_usleep(1);  /* ~0.45µs low */
        } else {
            /* 0-bit */
            gpio_pin_set_dt(&led_data, 1);
            k_usleep(0);  /* ~0.4µs high */
            gpio_pin_set_dt(&led_data, 0);
            k_usleep(1);  /* ~0.85µs low */
        }
    }
}

static void ws2812b_set_color(uint8_t r, uint8_t g, uint8_t b)
{
    /* WS2812B: GRB order */
    ws2812b_send_byte(g);
    ws2812b_send_byte(r);
    ws2812b_send_byte(b);
    k_usleep(60);  /* Reset pulse */
}

static void led_off(void)
{
    ws2812b_set_color(0, 0, 0);
}

static void led_red(void)
{
    ws2812b_set_color(255, 0, 0);
}

static void led_green(void)
{
    ws2812b_set_color(0, 255, 0);
}

static void led_blue(void)
{
    ws2812b_set_color(0, 0, 255);
}

/* ========================================================================
 * BLE ADVERTISING
 * ======================================================================== */

static void bt_ready(int err)
{
    if (err) {
        LOG_ERR("BLE init failed: %d", err);
        return;
    }
    
    LOG_INF("BLE ready");
    
    /* Start advertising */
    int ret = bt_le_adv_start(BT_LE_ADV_CONN_FAST, ad_data, ARRAY_SIZE(ad_data), NULL, 0);
    if (ret) {
        LOG_ERR("Advertising failed: %d", ret);
    } else {
        LOG_INF("BLE advertising started");
    }
}

static void update_advertising_data(void)
{
    /* Update manufacturer data */
    mfg_data[2] = g_tag.tag_id;
    mfg_data[3] = g_tag.battery_pct;
    mfg_data[4] = g_tag.panic_active ? 0x01 : 0x00;
    mfg_data[5] = g_tag.fall_detected ? 0x01 : 0x00;
    
    /* Restart advertising with updated data */
    bt_le_adv_stop();
    bt_le_adv_start(BT_LE_ADV_CONN_FAST, ad_data, ARRAY_SIZE(ad_data), NULL, 0);
}

/* ========================================================================
 * PANIC BUTTON HANDLER
 * ======================================================================== */

static uint32_t panic_press_start_ms = 0;

static void panic_btn_handler(const struct device *dev,
                               struct gpio_callback *cb,
                               uint32_t pins)
{
    if (gpio_pin_get_dt(&panic_btn) == 0) {
        /* Button pressed */
        panic_press_start_ms = k_uptime_get_32();
    } else {
        /* Button released */
        uint32_t hold_time_ms = k_uptime_get_32() - panic_press_start_ms;
        
        if (hold_time_ms >= 3000) {
            /* Long press (3s) = "I'm OK" cancel */
            LOG_INF("I'm OK cancel (hold=%dms)", hold_time_ms);
            g_tag.panic_active = false;
            g_tag.fall_detected = false;
            g_tag.state = TAG_STATE_CANCEL;
            beep_ok();
            led_green();
            k_msleep(500);
            led_off();
        } else if (hold_time_ms >= 50) {
            /* Short press = PANIC */
            LOG_WRN("PANIC BUTTON PRESSED (hold=%dms)", hold_time_ms);
            g_tag.panic_active = true;
            g_tag.state = TAG_STATE_PANIC;
            g_tag.panic_start_ms = k_uptime_get_32();
            beep_panic();
            led_red();
        }
    }
}

/* ========================================================================
 * BATTERY MONITORING
 * ======================================================================== */

static uint8_t read_battery_pct(void)
{
    /* Read battery voltage through voltage divider
     * CR2032: 3.0V fresh, ~2.0V dead
     * ADC reads through divider: VBAT * R2/(R1+R2)
     * R1=1MΩ, R2=1MΩ → ADC reads VBAT/2
     * ADC reference = 3.0V (nRF internal), 12-bit = 4095
     */
    
    /* In production: read ADC channel */
    /* Placeholder: return simulated value */
    uint8_t pct = 100 - (k_uptime_get_32() / 8640000);  /* Decrease 1% per day */
    if (pct > 100) pct = 5;  /* Minimum */
    return pct;
}

/* ========================================================================
 * FALL DETECTION FROM ACCELEROMETER
 * ======================================================================== */

static bool detect_fall(const accel_data_t *accel, float *magnitude_history, int history_len)
{
    /* Calculate acceleration magnitude */
    float magnitude = sqrtf(accel->x * accel->x + accel->y * accel->y + accel->z * accel->z);
    
    /* Check for free-fall (magnitude < 0.5g for >120ms) */
    int low_count = 0;
    for (int i = 0; i < history_len; i++) {
        if (magnitude_history[i] < 0.5f) low_count++;
    }
    
    /* Check for impact (magnitude > 2.5g) */
    bool impact = magnitude > 2.5f;
    
    /* Check for post-impact stillness (magnitude near 1g with low variance) */
    bool still = (magnitude > 0.85f && magnitude < 1.15f);
    
    /* Fall detection: free-fall → impact → stillness */
    if (low_count >= 2 && impact) {
        LOG_WRN("FALL PATTERN DETECTED: low_count=%d impact=%.2fg",
                low_count, magnitude);
        return true;
    }
    
    return false;
}

/* ========================================================================
 * MAIN LOOP
 * ======================================================================== */

int main(void)
{
    LOG_INF("=== HearthKeep Wearable Tag Starting ===");
    
    /* Initialize state */
    memset(&g_tag, 0, sizeof(g_tag));
    g_tag.tag_id = 0x01;  /* Default, set during pairing */
    g_tag.state = TAG_STATE_NORMAL;
    g_tag.battery_pct = read_battery_pct();
    
    /* Initialize GPIOs */
    gpio_pin_configure_dt(&led_data, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&piezo, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&panic_btn, GPIO_INPUT | GPIO_PULL_UP);
    
    /* Configure panic button interrupt */
    gpio_pin_interrupt_configure_dt(&panic_btn, GPIO_INT_EDGE_BOTH);
    gpio_init_callback(&panic_btn_cb, panic_btn_handler, BIT(panic_btn.pin));
    gpio_add_callback(panic_btn.port, &panic_btn_cb);
    
    /* Startup animation */
    led_blue();
    k_msleep(500);
    led_green();
    k_msleep(500);
    led_off();
    beep(200, 2000);
    
    /* Initialize BLE */
    int err = bt_enable(bt_ready);
    if (err) {
        LOG_ERR("BLE init failed: %d", err);
        /* Flash red LED rapidly */
        while (1) {
            led_red();
            k_msleep(100);
            led_off();
            k_msleep(100);
        }
    }
    
    /* Initialize accelerometer */
    const struct device *i2c = DEVICE_DT_GET(ACCEL_I2C_DEV);
    if (lis2dh12_init(i2c) != 0) {
        LOG_ERR("Accelerometer init failed!");
        led_red();
        while (1) { k_msleep(1000); }
    }
    
    LOG_INF("Wearable Tag running. Tag ID: 0x%02X", g_tag.tag_id);
    
    /* Main loop */
    uint32_t last_battery_check_ms = 0;
    uint32_t last_ad_update_ms = 0;
    uint32_t accel_idx = 0;
    
    while (1) {
        g_tag.uptime_ms = k_uptime_get_32();
        
        /* Read accelerometer */
        accel_data_t accel;
        if (lis2dh12_read(i2c, &accel) == 0) {
            g_tag.last_accel = accel;
            
            /* Update magnitude history */
            float magnitude = sqrtf(accel.x * accel.x + accel.y * accel.y + accel.z * accel.z);
            g_tag.accel_magnitude_history[g_tag.accel_history_idx] = magnitude;
            g_tag.accel_history_idx = (g_tag.accel_history_idx + 1) % 8;
            
            /* Check for free-fall interrupt */
            if (lis2dh12_check_freefall(i2c)) {
                LOG_WRN("Free-fall detected by LIS2DH12!");
            }
            
            /* Software fall detection */
            if (detect_fall(&accel, g_tag.accel_magnitude_history, 8)) {
                if (g_tag.state == TAG_STATE_NORMAL) {
                    g_tag.fall_detected = true;
                    g_tag.state = TAG_STATE_FALL_DETECTED;
                    g_tag.fall_start_ms = k_uptime_get_32();
                    beep_panic();
                    led_red();
                    LOG_WRN("FALL DETECTED by accelerometer!");
                }
            }
        }
        
        /* Handle panic state */
        if (g_tag.state == TAG_STATE_PANIC) {
            /* Continue beeping for 30 seconds or until cancelled */
            uint32_t elapsed = g_tag.uptime_ms - g_tag.panic_start_ms;
            if (elapsed < 30000) {
                /* Re-beep every 5 seconds */
                if (elapsed % 5000 < 100) {
                    beep(200, 2800);
                }
            } else {
                /* Auto-cancel after 30 seconds if no response */
                g_tag.panic_active = false;
                g_tag.state = TAG_STATE_NORMAL;
                led_off();
            }
        }
        
        /* Handle fall detected state */
        if (g_tag.state == TAG_STATE_FALL_DETECTED) {
            uint32_t elapsed = g_tag.uptime_ms - g_tag.fall_start_ms;
            if (elapsed < 60000) {
                /* Flash red LED */
                if ((elapsed / 500) % 2 == 0) {
                    led_red();
                } else {
                    led_off();
                }
            } else {
                /* Auto-cancel after 60 seconds */
                g_tag.fall_detected = false;
                g_tag.state = TAG_STATE_NORMAL;
                led_off();
            }
        }
        
        /* Handle cancel state */
        if (g_tag.state == TAG_STATE_CANCEL) {
            /* Brief green flash then back to normal */
            g_tag.state = TAG_STATE_NORMAL;
        }
        
        /* Update battery percentage every 60 seconds */
        if (g_tag.uptime_ms - last_battery_check_ms > 60000) {
            g_tag.battery_pct = read_battery_pct();
            last_battery_check_ms = g_tag.uptime_ms;
            
            if (g_tag.battery_pct <= 15) {
                LOG_WRN("Low battery: %d%%", g_tag.battery_pct);
                beep_low_battery();
                g_tag.state = TAG_STATE_LOW_BATTERY;
            }
        }
        
        /* Update BLE advertising data every 2 seconds */
        if (g_tag.uptime_ms - last_ad_update_ms > 2000) {
            update_advertising_data();
            last_ad_update_ms = g_tag.uptime_ms;
        }
        
        /* Sleep between samples (25Hz = 40ms) */
        k_msleep(40);
    }
    
    return 0;
}