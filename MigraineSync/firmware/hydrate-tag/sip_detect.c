/**
 * MigraineSync — Hydrate Tag Sip Detection
 * ========================================
 * Combines LSM6DSO tilt detection + HX711 load cell delta
 * to classify drinking events (sip vs pour vs spill).
 *
 * License: MIT
 */

#include "sip_detect.h"
#include "config.h"
#include "loadcell.h"

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <math.h>

LOG_MODULE_REGISTER(migrainesync_sip, LOG_LEVEL_INF);

static const struct device *i2c_dev;

/* ── Sip state ──────────────────────────────────────────── */
static float s_last_weight_g = 0.0f;
static float s_daily_intake_ml = 0.0f;
static uint8_t s_sip_count = 0;
static int64_t s_last_sip_time = 0;
static float s_midnight_weight = 0.0f;

/* ── Init ───────────────────────────────────────────────── */
int sip_detect_init(void)
{
    i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
    if (!device_is_ready(i2c_dev))
        return -1;

    /* Init LSM6DSO: accelerometer ±2g, 12.5 Hz (low power for wake) */
    uint8_t cfg[2];

    /* CTRL1_XL = 0x10: ODR=12.5Hz, FS=±2g */
    cfg[0] = 0x10; cfg[1] = 0x20;
    i2c_write(i2c_dev, cfg, 2, LSM6DSO_ADDR);

    /* INT1_CTRL = 0x0D: enable wake-up + free-fall + activity */
    cfg[0] = 0x0D; cfg[1] = 0x2D;
    i2c_write(i2c_dev, cfg, 2, LSM6DSO_ADDR);

    /* TAP_CFG = 0x58: enable tilt + slope */
    cfg[0] = 0x58; cfg[1] = 0x90;
    i2c_write(i2c_dev, cfg, 2, LSM6DSO_ADDR);

    /* Wake-up threshold = 0x5B: 63 mg */
    cfg[0] = 0x5B; cfg[1] = 0x02;
    i2c_write(i2c_dev, cfg, 2, LSM6DSO_ADDR);

    /* Get initial weight as baseline */
    loadcell_power_up();
    s_last_weight_g = loadcell_read_grams();
    s_midnight_weight = s_last_weight_g;

    LOG_INF("Sip detection initialized (baseline weight: %.1fg)", s_last_weight_g);
    return 0;
}

/* ── Read tilt angle from accelerometer ─────────────────── */
static float read_tilt_degrees(void)
{
    uint8_t data[6];
    uint8_t reg = 0x28;  /* OUTX_L_A */
    int ret = i2c_write_read(i2c_dev, LSM6DSO_ADDR, &reg, 1, data, 6);
    if (ret != 0)
        return 0.0f;

    int16_t ax = (int16_t)((data[0] << 8) | data[1]);
    int16_t ay = (int16_t)((data[2] << 8) | data[3]);
    int16_t az = (int16_t)((data[4] << 8) | data[5]);

    /* Convert to g (±2g range, 0.061 mg/LSB) */
    float x = ax * 0.061f / 1000.0f;
    float y = ay * 0.061f / 1000.0f;
    float z = az * 0.061f / 1000.0f;

    /* Tilt angle from vertical (gravity vector) */
    float mag = sqrtf(x*x + y*y + z*z);
    if (mag < 0.01f)
        return 0.0f;

    /* Angle between z-axis and gravity → tilt */
    float cos_tilt = z / mag;
    if (cos_tilt > 1.0f) cos_tilt = 1.0f;
    if (cos_tilt < -1.0f) cos_tilt = -1.0f;

    return acosf(cos_tilt) * 180.0f / 3.14159265f;
}

/* ── Detect sip event ───────────────────────────────────── */
sip_event_t sip_detect_poll(void)
{
    /* Read tilt */
    float tilt = read_tilt_degrees();

    /* If not tilted, no sip happening */
    if (tilt < SIP_TILT_THRESHOLD_DEG)
        return SIP_EVENT_NONE;

    /* Tilted — measure weight change */
    float current_weight = loadcell_read_grams();
    if (current_weight < 0)
        return SIP_EVENT_NONE;

    float weight_delta = s_last_weight_g - current_weight;

    /* Check timing (avoid double-counting) */
    int64_t now = k_uptime_get();
    if (now - s_last_sip_time < SIP_MIN_INTERVAL_MS)
        return SIP_EVENT_NONE;

    /* Classify based on weight change */
    if (weight_delta >= SIP_WEIGHT_DELTA_G) {
        /* Real sip — weight decreased significantly while tilted */
        float sip_ml = weight_delta;  /* 1g ≈ 1ml for water */
        s_daily_intake_ml += sip_ml;
        s_sip_count++;
        s_last_sip_time = now;
        s_last_weight_g = current_weight;

        LOG_INF("SIP detected: %.1fml (daily total: %.1fml, sips: %u)",
                sip_ml, s_daily_intake_ml, s_sip_count);

        /* Determine if this was a large gulp or normal sip */
        if (sip_ml > 50.0f)
            return SIP_EVENT_GULP;
        return SIP_EVENT_SIP;
    }
    else if (weight_delta < -5.0f) {
        /* Weight increased — bottle was refilled */
        s_last_weight_g = current_weight;
        LOG_INF("Bottle refilled (+%.1fg)", -weight_delta);
        return SIP_EVENT_REFILL;
    }
    else if (weight_delta < -20.0f) {
        /* Large weight decrease without proper tilt — possible spill */
        s_last_weight_g = current_weight;
        LOG_WRN("Possible spill detected (-%.1fg)", -weight_delta);
        return SIP_EVENT_SPILL;
    }

    return SIP_EVENT_NONE;
}

/* ── Get daily intake summary ───────────────────────────── */
float sip_get_daily_intake_ml(void)
{
    return s_daily_intake_ml;
}

uint8_t sip_get_sip_count(void)
{
    return s_sip_count;
}

float sip_get_bottle_weight_g(void)
{
    return loadcell_read_grams();
}

/* ── Reset daily counter (called at midnight) ───────────── */
void sip_reset_daily(void)
{
    s_daily_intake_ml = 0.0f;
    s_sip_count = 0;
    s_midnight_weight = loadcell_read_grams();
    LOG_INF("Daily counters reset");
}