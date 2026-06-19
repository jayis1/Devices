/*
 * ankle_main.c — SoleGuard Ankle Wearable Tag firmware (nRF52840, Zephyr)
 *
 * IMU gait + AD5940 bioimpedance (edema) + TMP117 skin temp.
 * Acts as a BLE mesh relay between insoles and hub.
 * Fall detection -> immediate mesh-flood alert.
 *
 * SPDX-License-Identifier: MIT
 */
#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include "sole_protocol.h"

LOG_MODULE_REGISTER(ankle, LOG_LEVEL_INF);

#define NODE_ID SOLE_NODE_ID_ANKLE

static const struct device *imu_dev;
static const struct device *temp_dev;   /* TMP117 on I2C */

/* ---- AD5940 bioimpedance (SPI) ---- */
extern int  bioz_init(const struct device *spi);
extern int  bioz_measure(uint16_t *impedance_ohm, uint16_t *edema_index);
extern void bioz_sleep(void);

/* ---- Fall detection ---- */
static void check_fall(float accel_mag_mps2)
{
    static int64_t last_freefall_ms = 0;
    static int in_freefall = 0;
    int64_t t = k_uptime_get();

    if (accel_mag_mps2 < 3.0f && !in_freefall) {
        in_freefall = 1;
        last_freefall_ms = t;
    } else if (in_freefall && accel_mag_mps2 > 18.0f) {
        /* free-fall followed by high impact within 1s = fall */
        if (t - last_freefall_ms < 1000) {
            sole_alert_payload_t a = {0};
            a.type     = SOLE_MSG_ALERT;
            a.node_id  = NODE_ID;
            a.seq      = 0;
            a.flags    = SOLE_ALERT_FALL;
            a.value    = (uint16_t)(accel_mag_mps2 * 10.0f);
            sole_pack_crc(&a, sizeof(a) - 2);
            extern int sole_mesh_publish_model0(const void *data, size_t len);
            sole_mesh_publish_model0(&a, sizeof(a));
            LOG_ERR("FALL DETECTED — alert published");
        }
        in_freefall = 0;
    } else if (in_freefall && t - last_freefall_ms > 1500) {
        in_freefall = 0;
    }
}

static void sample_imu(float *az, float *gy, float *mag)
{
    struct sensor_value a[3], g[3];
    if (sensor_sample_fetch(imu_dev)) return;
    sensor_channel_get(imu_dev, SENSOR_CHAN_ACCEL_XYZ, a);
    sensor_channel_get(imu_dev, SENSOR_CHAN_GYRO_XYZ, g);
    *az = sensor_value_to_double(&a[2]);
    *gy = sensor_value_to_double(&g[1]);
    float ax = sensor_value_to_double(&a[0]);
    float ay = sensor_value_to_double(&a[1]);
    *mag = sqrtf(ax*ax + ay*ay + (*az)*(*az));
}

int main(void)
{
    imu_dev  = DEVICE_DT_GET(DT_NODELABEL(lsm6dso32));
    temp_dev = DEVICE_DT_GET(DT_NODELABEL(tmp117));
    const struct device *spi = DEVICE_DT_GET(DT_NODELABEL(spi1));

    if (!device_is_ready(imu_dev) || !device_is_ready(temp_dev)) {
        LOG_ERR("IMU or TMP117 not ready");
        return -1;
    }
    bioz_init(spi);

    LOG_INF("SoleGuard ankle tag starting");

    uint8_t seq = 0;
    int64_t last_bioz = 0;
    int64_t last_report = 0;
    uint16_t edema_index = 0;
    uint16_t impedance = 0;
    int16_t skin_temp = 0;

    while (1) {
        float az, gy, mag;
        sample_imu(&az, &gy, &mag);
        check_fall(mag);

        /* Bioimpedance every 4 hours (edema is slow) */
        if (k_uptime_get() - last_bioz > 4 * 3600 * 1000) {
            bioz_measure(&impedance, &edema_index);
            bioz_sleep();
            last_bioz = k_uptime_get();
            LOG_INF("BioZ: %u ohm, edema_idx=%u", impedance, edema_index);
        }

        /* Skin temp every 20s */
        struct sensor_value t;
        if (sensor_sample_fetch(temp_dev) == 0 &&
            sensor_channel_get(temp_dev, SENSOR_CHAN_AMBIENT_TEMP, &t) == 0) {
            skin_temp = (int16_t)(sensor_value_to_double(&t) * 100.0f);
        }

        /* Report gait + edema every 60s */
        if (k_uptime_get() - last_report > 60000) {
            sole_gait_payload_t gp = {0};
            gp.type = SOLE_MSG_GAIT;
            gp.node_id = NODE_ID;
            gp.seq = seq;
            gp.gait[5] = (int16_t)((az / G) * 1000.0f); /* foot clearance proxy */
            sole_pack_crc(&gp, sizeof(gp) - 2);
            extern int sole_mesh_publish_model0(const void *data, size_t len);
            sole_mesh_publish_model0(&gp, sizeof(gp));

            sole_edema_payload_t ep = {0};
            ep.type = SOLE_MSG_EDEMA;
            ep.node_id = NODE_ID;
            ep.seq = seq;
            ep.impedance_ohm = impedance;
            ep.edema_index = edema_index;
            ep.skin_temp_centic = skin_temp;
            if (edema_index > 650)
                ep.flags = SOLE_ALERT_EDEMA;
            sole_pack_crc(&ep, sizeof(ep) - 2);
            sole_mesh_publish_model0(&ep, sizeof(ep));

            seq++;
            last_report = k_uptime_get();
        }
        k_msleep(100); /* 10 Hz IMU is enough for gait + fall */
    }
    return 0;
}