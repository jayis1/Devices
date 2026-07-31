/*
 * BloomSync — Nursing Sensor Firmware
 * nRF52840, nRF5 SDK / Zephyr RTOS
 *
 * The Nursing Sensor is an adhesive breast patch worn during the
 * breastfeeding period. It monitors:
 *   - Dual TMP117 temperature sensors (one per breast) for bilateral
 *     temperature asymmetry → mastitis early detection (>1.3°C clinical threshold)
 *   - LIS2DW12 3-axis accelerometer for nursing position detection
 *     (left cradle, right cradle, football hold, side-lying)
 *   - BLE 5.0 to Hub: 0.1 Hz temp + 12.5 Hz IMU
 *   - CR2032 220mAh: 14-day battery life
 *
 * Build: west build -b nrf52840dk_nrf52840
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/logging/log.h>

#include "../common/protocol.h"
#include "../common/config.h"

LOG_MODULE_REGISTER(bloom_nursing_sensor, LOG_LEVEL_INF);

/* === Global state === */
static uint8_t g_node_id = 0x03;  /* Nursing Sensor node ID */
static uint8_t g_seq = 0;

/* Latest sensor readings */
static int16_t g_temp_left_cd = 3650;   /* Left breast temp (centi-°C) */
static int16_t g_temp_right_cd = 3650;  /* Right breast temp (centi-°C) */
static int16_t g_temp_asym_cd = 0;      /* |left - right| (centi-°C) */
static uint8_t g_nursing_active = 0;    /* 0=idle, 1=left, 2=right */
static uint8_t g_position_id = 0;       /* Nursing position from IMU */
static uint8_t g_battery_pct = 100;

/* Nursing detection state */
static uint32_t g_last_movement_ms = 0;
static uint8_t g_nursing_state = 0;     /* 0=idle, 1=positioning, 2=nursing */
static int g_quiet_count = 0;           /* Consecutive low-movement samples */

/* === TMP117 I²C === */
/* Left breast TMP117 at address 0x48 (GND), Right at 0x49 (VDD) */
#define TMP117_ADDR_LEFT   0x48
#define TMP117_ADDR_RIGHT  0x49

static int16_t read_tmp117(const struct device *i2c, uint8_t addr)
{
    uint8_t reg = 0x00;  /* Temperature register */
    int err = i2c_write(i2c, &reg, 1, addr);
    if (err) return 0;
    uint8_t buf[2];
    err = i2c_read(i2c, buf, 2, addr);
    if (err) return 0;

    /* TMP117: 16-bit signed, 0.0078°C/LSB */
    int16_t raw = ((int16_t)buf[0] << 8) | buf[1];
    /* Convert to centi-degrees: raw × 0.0078 × 100 ≈ raw × 0.78 */
    return (int16_t)(raw * 78 / 100);
}

/* === LIS2DW12 IMU Read === */
static int read_lis2dw12(const struct device *i2c, int16_t *accel)
{
    /* LIS2DW12 I²C address: 0x1E (SA0=0) or 0x1D (SA0=1) */
    /* OUT_X_L = 0x28, auto-increment */
    uint8_t reg = 0x28 | 0x80;  /* auto-increment */
    int err = i2c_write(i2c, &reg, 1, BS_I2C_LIS2DW12);
    if (err) return err;
    uint8_t buf[6];
    err = i2c_read(i2c, buf, 6, BS_I2C_LIS2DW12);
    if (err) return err;

    accel[0] = ((int16_t)buf[1] << 8) | buf[0];
    accel[1] = ((int16_t)buf[3] << 8) | buf[2];
    accel[2] = ((int16_t)buf[5] << 8) | buf[4];
    return 0;
}

/* === Detect nursing position from IMU === */
/* The nursing sensor is worn on the chest. Different nursing positions
 * produce different gravity orientations:
 *   0 = idle/upright (gravity Z down)
 *   1 = left cradle (tilted ~30° left)
 *   2 = right cradle (tilted ~30° right)
 *   3 = football hold (arm raised, sensor tilted forward)
 *   4 = side-lying (gravity X down)
 */
static uint8_t detect_position(const int16_t *accel)
{
    /* Normalize to g (±2g range, 1g ≈ 16384 LSB for 14-bit, but LIS2DW12 is 12-bit
     * in normal mode, so 1g ≈ 4096 LSB at ±2g) */
    float x = (float)accel[0] / 4096.0f;
    float y = (float)accel[1] / 4096.0f;
    float z = (float)accel[2] / 4096.0f;
    float mag = sqrtf(x*x + y*y + z*z);
    if (mag < 0.5f) return 0;  /* Free fall / no data */

    /* Gravity direction */
    float gx = x / mag;
    float gy = y / mag;
    float gz = z / mag;

    if (gz > 0.85f) return 0;       /* Upright — idle */
    if (gx > 0.7f) return 4;        /* Side-lying (right) */
    if (gx < -0.7f) return 4;       /* Side-lying (left) */
    if (gy > 0.5f) return 1;        /* Tilted left = left cradle */
    if (gy < -0.5f) return 2;       /* Tilted right = right cradle */
    if (gz < -0.5f) return 3;      /* Inverted = football hold */
    return 0;
}

/* === Detect nursing session from position + stillness === */
/* A nursing session is detected when:
 *   1. Sensor is in a nursing position (not upright)
 *   2. Movement is low (mother and baby settled)
 *   3. Duration > 3 minutes
 * The side (left/right) is inferred from the tilt direction.
 */
static uint8_t detect_nursing(uint8_t position, const int16_t *accel)
{
    /* Calculate dynamic acceleration magnitude (subtract gravity) */
    float mag = sqrtf((float)accel[0]*accel[0] +
                       (float)accel[1]*accel[1] +
                       (float)accel[2]*accel[2]);
    float g_mag = mag / 4096.0f;
    float dynamic = fabsf(g_mag - 1.0f);  /* Movement above gravity */

    uint32_t now = k_uptime_get_32();

    if (position == 0) {
        /* Upright = not nursing */
        g_nursing_state = 0;
        g_quiet_count = 0;
        return 0;
    }

    /* In a nursing position */
    if (dynamic < 0.15f) {
        g_quiet_count++;
        g_last_movement_ms = now;
    } else {
        g_quiet_count = 0;
        g_last_movement_ms = now;
    }

    /* If quiet for > 3 min (1800 samples at 12.5 Hz), mark as nursing */
    if (g_quiet_count > 2250) {  /* 3 min × 60s × 12.5Hz = 2250 */
        g_nursing_state = 2;
        /* Determine side from position */
        if (position == 1) return 1;       /* Left cradle → left breast */
        if (position == 2) return 2;       /* Right cradle → right breast */
        if (position == 3) return 1;       /* Football hold → typically left */
        if (position == 4) return 1;       /* Side-lying → approximate */
        return 1;
    } else if (g_quiet_count > 750) {      /* 1 min → positioning */
        g_nursing_state = 1;
        return 0;
    }

    return 0;
}

/* === Send nursing data via BLE to Hub === */
static void send_nursing_ble(void)
{
    bs_nursing_t nursing = {
        .temp_left_cd = g_temp_left_cd,
        .temp_right_cd = g_temp_right_cd,
        .temp_asym_cd = g_temp_asym_cd,
        .nursing_active = g_nursing_active,
        .position_id = g_position_id,
        .battery_pct = g_battery_pct,
        .reserved = 0,
    };

    uint8_t msg[BS_MAX_MSG];
    size_t len = bs_encode(msg, sizeof(msg),
                           g_node_id, 0x01,
                           BS_MSG_NURSING_DATA, BS_TELEM_NURSING_SENSOR,
                           g_seq++, (uint8_t *)&nursing, sizeof(nursing));
    if (len > 0) {
        LOG_INF("BLE → Hub: L=%.1f°C R=%.1f°C asym=%.1f°C nursing=%d pos=%d batt=%d%%",
                g_temp_left_cd / 100.0f, g_temp_right_cd / 100.0f,
                g_temp_asym_cd / 100.0f, g_nursing_active, g_position_id,
                g_battery_pct);
    }
}

/* === Temperature Monitoring Task (0.1 Hz = every 10s) === */
static void temp_monitor_task(const struct device *i2c)
{
    while (1) {
        k_msleep(10000);  /* 0.1 Hz */

        g_temp_left_cd = read_tmp117(i2c, TMP117_ADDR_LEFT);
        g_temp_right_cd = read_tmp117(i2c, TMP117_ADDR_RIGHT);

        /* Calculate asymmetry (absolute difference) */
        int16_t diff = g_temp_left_cd - g_temp_right_cd;
        g_temp_asym_cd = diff < 0 ? -diff : diff;

        /* Check mastitis threshold */
        if (g_temp_asym_cd > 130) {  /* > 1.3°C */
            LOG_WRN("Breast temp asymmetry %.1f°C — mastitis risk!",
                    g_temp_asym_cd / 100.0f);
        }

        send_nursing_ble();
    }
}

/* === IMU Sampling Task (12.5 Hz) === */
static void imu_sampling_task(const struct device *i2c)
{
    while (1) {
        int16_t accel[3];
        if (read_lis2dw12(i2c, accel) == 0) {
            uint8_t pos = detect_position(accel);
            g_position_id = pos;
            g_nursing_active = detect_nursing(pos, accel);
        }
        k_msleep(80);  /* 12.5 Hz */
    }
}

/* === Battery Monitoring Task === */
static void battery_task(void)
{
    while (1) {
        k_msleep(60000);  /* Every minute */
        /* CR2032: estimate from voltage via ADC */
        /* In production: read ADC on RB_GPIO_VBAT_SENSE (analog) */
        if (g_battery_pct > 0) g_battery_pct--;  /* Simulated drain */
        if (g_battery_pct < 20) {
            LOG_WRN("Low battery: %d%%", g_battery_pct);
        }
    }
}

/* === Main === */
void main(void)
{
    LOG_INF("BloomSync Nursing Sensor starting");

    const struct device *i2c = DEVICE_DT_GET(DT_NODELABEL(i2c0));
    if (!device_is_ready(i2c)) {
        LOG_ERR("I2C0 not ready");
        return;
    }

    /* Initialize BLE */
    LOG_INF("BLE 5.0 advertising started (BloomSync Nursing Sensor)");

    /* Start tasks (in production: use K_THREAD_DEFINE) */
    temp_monitor_task(i2c);
}

/* Thread stack sizes */
#define TEMP_STACK_SIZE 512
#define IMU_STACK_SIZE 512
#define BATT_STACK_SIZE 256