/*
 * WanderSync — Wander Band Firmware
 * nRF52840, Zephyr RTOS
 *
 * The Wander Band is worn by the person with dementia. It tracks GPS
 * location with geofencing, recognizes activity via 9-DoF IMU, monitors
 * heart rate via PPG, runs WanderNet lite LSTM for wandering risk
 * prediction, detects falls, has an SOS button, and communicates
 * over Sub-GHz (2+ km range) with BLE 5.0 fallback.
 *
 * Build: west build -b nrf52840dk_nrf52840
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/sys/printk.h>

#include "../common/protocol.h"
#include "../common/subghz_mesh.h"
#include "../common/config.h"

#define TAG "WanderSync-Band"

/* === Global state === */
static ws_mesh_ctx_t g_mesh;
static ws_radio_hal_t g_radio_hal;
static ws_radio_config_t g_radio_cfg;
static ws_geofence_t g_geofence;

/* GPS state */
static int32_t g_gps_lat_e7 = 0;
static int32_t g_gps_lon_e7 = 0;
static uint8_t g_gps_fix = 0;
static uint8_t g_gps_outdoor = 0; /* Detected via Sub-GHz RSSI */

/* IMU state */
static float g_accel_x, g_accel_y, g_accel_z;
static float g_gyro_x, g_gyro_y, g_gyro_z;
static uint8_t g_activity_class = WS_ACT_SITTING;
static uint16_t g_step_count = 0;
static uint8_t g_fall_detected = 0;
static uint8_t g_impact_g_x10 = 0;
static int64_t g_fall_timestamp = 0;
static int64_t g_last_movement_time = 0;

/* PPG state */
static uint8_t g_hr_bpm = 0;
static uint16_t g_hrv_ms = 0;

/* Wander state */
static uint8_t g_wander_risk = 0;
static uint8_t g_sos_pressed = 0;
static uint8_t g_band_on_wrist = 1;
static int64_t g_sos_timestamp = 0;

/* === NMEA GPS Parsing (L80-R) === */
static const struct device *uart_gps;

static void gps_uart_cb(const struct device *dev, void *user_data)
{
    /* Production: parse $GPGGA, $GPRMC sentences for lat/lon/fix */
    /* $GPGGA,hhmmss.ss,llll.ll,a,yyyyy.yy,a,x,xx,h.h,m,m,*hh */
    /* Simplified: set fix when we get valid GGA */
}

static void gps_init(void)
{
    uart_gps = DEVICE_DT_GET(DT_NODELABEL(uart1));
    if (!device_is_ready(uart_gps)) {
        printk("GPS UART not ready\n");
        return;
    }
    uart_irq_callback_set(uart_gps, gps_uart_cb);
    uart_irq_rx_enable(uart_gps);

    /* Enable GPS via GPIO */
    /* gpio_pin_set(gps_en_dev, BAND_PIN_GPS_EN, 1); */
    printk("GPS L80-R initialized\n");
}

/* Parse NMEA $GPGGA sentence */
static int gps_parse_gga(const char *sentence, int32_t *lat_e7, int32_t *lon_e7,
                         uint8_t *fix)
{
    /* $GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47 */
    /* Production: full NMEA parser */
    *fix = 1;
    *lat_e7 = 374449000;   /* Placeholder — production: parse from sentence */
    *lon_e7 = -1224159000;
    return 0;
}

/* === IMU (LSM6DSL) — Activity Recognition + Fall Detection === */
static const struct device *i2c_dev;

static void imu_read(float *ax, float *ay, float *az,
                     float *gx, float *gy, float *gz)
{
    /* Production: read LSM6DSL registers via I²C */
    /* OUTX_L_G (0x22), OUTX_L_A (0x28) */
    /* Accel: ±4g range, 0.122 mg/LSB */
    /* Gyro: ±2000 dps, 70 mdps/LSB */
    *ax = 0.0; *ay = 0.0; *az = 9.8; /* Placeholder: 1g down */
    *gx = 0.0; *gy = 0.0; *gz = 0.0;
}

static uint8_t imu_classify_activity(float ax, float ay, float az,
                                      float gx, float gy, float gz)
{
    /* Simple heuristic activity classifier (production: WanderNet lite LSTM) */
    float mag = sqrtf(ax * ax + ay * ay + az * az);
    float gyro_mag = sqrtf(gx * gx + gy * gy + gz * gz);

    if (gyro_mag > 2.0 && mag > 1.2) return WS_ACT_WALKING;
    if (mag < 0.5) return WS_ACT_LYING;
    if (mag > 0.8 && mag < 1.2 && gyro_mag < 0.5) return WS_ACT_SITTING;
    if (mag > 0.9 && mag < 1.1 && gyro_mag < 0.3) return WS_ACT_STANDING;
    if (gyro_mag > 0.5 && gyro_mag < 2.0) return WS_ACT_FIDGETING;
    return WS_ACT_SITTING;
}

static void imu_check_fall(float ax, float ay, float az, int64_t now)
{
    float mag = sqrtf(ax * ax + ay * ay + az * az);
    float g = mag / 9.8;

    /* Fall: impact > 3g followed by stillness */
    if (g * 10 > WS_FALL_IMPACT_G) {
        g_impact_g_x10 = (uint8_t)(g * 10);
        g_fall_timestamp = now;
        g_fall_detected = 1; /* Pending stillness confirmation */
        printk("FALL: impact=%.1fg detected\n", g);
    }

    /* Track movement for stillness check */
    if (g > 0.3 && g < 1.5) {
        g_last_movement_time = now;
    }

    /* Confirm fall: stillness for 30 seconds after impact */
    if (g_fall_detected && (now - g_fall_timestamp) > WS_FALL_STILLNESS_MS) {
        if ((now - g_last_movement_time) > WS_FALL_STILLNESS_MS) {
            printk("FALL CONFIRMED: stillness >30s after impact\n");
            g_activity_class = WS_ACT_FALL;
        } else {
            g_fall_detected = 0; /* Recovered from fall */
        }
    }
}

/* === PPG (MAX30101) — Heart Rate === */
static void ppg_read_hr(uint8_t *hr, uint16_t *hrv)
{
    /* Production: read MAX30101 FIFO, compute HR via autocorrelation,
     * HRV via RMSSD of peak-to-peak intervals */
    *hr = 72; /* Placeholder */
    *hrv = 45;
}

/* === WanderNet Lite — On-Device Wandering Risk (simplified) === */
static uint8_t wandernet_lite_predict(int32_t lat_e7, int32_t lon_e7,
                                       uint8_t activity, uint8_t hour,
                                       uint8_t geofence_status)
{
    /* Production: TFLite-Micro int8 LSTM inference (~120 KB)
     * Inputs: 12-hour GPS trajectory, activity, time-of-day, history
     * Output: wandering risk 0-100
     *
     * Simplified heuristic: */
    uint8_t risk = 0;

    /* Outside geofence = high risk */
    if (geofence_status == 1) risk += 40;
    else if (geofence_status == 2) risk += 20;

    /* Nighttime walking = high risk */
    if (hour >= 22 || hour < 6) {
        if (activity == WS_ACT_WALKING) risk += 35;
        else risk += 10;
    }

    /* Active movement near boundary = elevated */
    if (activity == WS_ACT_WALKING && geofence_status == 2) risk += 15;

    if (risk > 100) risk = 100;
    return risk;
}

/* === Geofence Check === */
static double ws_haversine_m(int32_t lat1_e7, int32_t lon1_e7,
                             int32_t lat2_e7, int32_t lon2_e7)
{
    double lat1 = lat1_e7 / 1e7;
    double lon1 = lon1_e7 / 1e7;
    double lat2 = lat2_e7 / 1e7;
    double lon2 = lon2_e7 / 1e7;
    double dlat = (lat2 - lat1) * M_PI / 180.0;
    double dlon = (lon2 - lon1) * M_PI / 180.0;
    double a = sin(dlat / 2) * sin(dlat / 2) +
               cos(lat1 * M_PI / 180.0) * cos(lat2 * M_PI / 180.0) *
               sin(dlon / 2) * sin(dlon / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    return 6371000.0 * c;
}

static uint8_t ws_check_geofence(int32_t lat_e7, int32_t lon_e7)
{
    if (g_geofence.radius_m == 0) return 0; /* No geofence set */
    double dist = ws_haversine_m(lat_e7, lon_e7,
                                  g_geofence.center_lat_e7,
                                  g_geofence.center_lon_e7);
    if (dist > g_geofence.radius_m) return 1;
    if (dist > g_geofence.radius_m - WS_GEOFENCE_NEAR_M) return 2;
    return 0;
}

/* === SX1262 HAL (nRF52840 SPI) === */
/* Production: Zephyr SPI driver bindings */
static int hal_spi_init(void) { return 0; }
static int hal_spi_xfer(const uint8_t *tx, uint8_t *rx, size_t len) { return 0; }
static void hal_cs_low(void) {}
static void hal_cs_high(void) {}
static void hal_reset(int assert) {}
static int  hal_dio1_read(void) { return 0; }
static int  hal_busy_read(void) { return 0; }
static void hal_delay_ms(uint32_t ms) { k_msleep(ms); }
static void hal_delay_us(uint32_t us) { k_busy_wait(us); }
static void hal_on_dio1(void) {}

/* === Send Alert === */
static void send_alert(uint8_t alert_type)
{
    ws_message_t msg;
    ws_build_wander_alert(&msg, g_mesh.node_id, g_mesh.msg_counter++,
                          alert_type, g_gps_lat_e7, g_gps_lon_e7,
                          g_wander_risk, g_activity_class,
                          ws_check_geofence(g_gps_lat_e7, g_gps_lon_e7),
                          g_mesh.battery_v, g_impact_g_x10,
                          g_band_on_wrist, g_hr_bpm);

    ws_mesh_send_emergency(&g_mesh, &g_radio_hal, &msg);

    /* Haptic feedback */
    /* drv2605l_vibrate(alert_type == WS_ALERT_TYPE_SOS ? 3 : 1); */

    printk("ALERT sent: type=%d lat=%d lon=%d\n",
           alert_type, g_gps_lat_e7, g_gps_lon_e7);
}

/* === Send Telemetry === */
static void send_telemetry(void)
{
    ws_band_telem_t telem = {
        .subtype = WS_TELEM_BAND,
        .battery_v = g_mesh.battery_v,
        .gps_lat_e7 = g_gps_lat_e7,
        .gps_lon_e7 = g_gps_lon_e7,
        .gps_fix = g_gps_fix,
        .activity_class = g_activity_class,
        .wander_risk = g_wander_risk,
        .hr_bpm = g_hr_bpm,
        .hrv_ms = g_hrv_ms,
        .steps = g_step_count,
        .geofence_status = ws_check_geofence(g_gps_lat_e7, g_gps_lon_e7),
        .distance_home_m = (uint16_t)ws_haversine_m(
            g_gps_lat_e7, g_gps_lon_e7,
            g_geofence.center_lat_e7, g_geofence.center_lon_e7),
        .band_on_wrist = g_band_on_wrist,
        .free_heap = 0, /* Production: k_heap_free() */
        .rssi = g_mesh.last_rssi,
        .uptime_min = (uint16_t)(k_uptime_get() / 60000),
        .sos_pressed = g_sos_pressed,
    };

    ws_message_t msg;
    ws_build_band_telem(&msg, g_mesh.node_id, g_mesh.msg_counter++, &telem);
    ws_mesh_send(&g_mesh, &g_radio_hal, &msg, WS_SEV_INFO);

    g_sos_pressed = 0; /* Reset SOS flag after telemetry */
}

/* === SOS Button Callback === */
static void sos_callback(const struct device *port,
                          struct gpio_callback *cb, uint32_t pins)
{
    int64_t now = k_uptime_get();
    if (now - g_sos_timestamp < BAND_SOS_DEBOUNCE_MS) return;
    g_sos_timestamp = now;
    g_sos_pressed = 1;
    printk("SOS BUTTON PRESSED!\n");
    send_alert(WS_ALERT_TYPE_SOS);
}

/* === Band Tamper Callback === */
static void tamper_callback(const struct device *port,
                             struct gpio_callback *cb, uint32_t pins)
{
    g_band_on_wrist = 0;
    printk("BAND REMOVED!\n");
    send_alert(WS_ALERT_TYPE_BAND_OFF);
}

/* === GPS Task === */
static void gps_task(void *a, void *b, void *c)
{
    while (1) {
        /* Read GPS (duty-cycled: 1 Hz outdoor, 0.1 Hz indoor) */
        uint32_t interval = g_gps_outdoor ? BAND_GPS_OUTDOOR_MS :
                                             BAND_GPS_INDOOR_MS;

        /* Production: enable GPS, wait for fix, parse NMEA, disable GPS */
        /* Simulated: use placeholder values */
        g_gps_fix = 1;
        g_gps_lat_e7 = 374449000 + (int32_t)(k_random() % 1000 - 500);
        g_gps_lon_e7 = -1224159000 + (int32_t)(k_random() % 1000 - 500);

        /* Determine outdoor from Sub-GHz RSSI */
        g_gps_outdoor = (g_mesh.last_rssi < -90) ? 1 : 0;

        k_msleep(interval);
    }
}

/* === IMU Task (50 Hz) === */
static void imu_task(void *a, void *b, void *c)
{
    int64_t last_step = 0;
    while (1) {
        imu_read(&g_accel_x, &g_accel_y, &g_accel_z,
                 &g_gyro_x, &g_gyro_y, &g_gyro_z);

        int64_t now = k_uptime_get();

        /* Activity classification */
        g_activity_class = imu_classify_activity(
            g_accel_x, g_accel_y, g_accel_z,
            g_gyro_x, g_gyro_y, g_gyro_z);

        /* Fall detection */
        imu_check_fall(g_accel_x, g_accel_y, g_accel_z, now);

        /* Step counting (simplified: count walking cycles) */
        if (g_activity_class == WS_ACT_WALKING) {
            if (now - last_step > 500) { /* ~2 steps/sec */
                g_step_count++;
                last_step = now;
            }
        }

        /* Check for confirmed fall */
        if (g_activity_class == WS_ACT_FALL) {
            send_alert(WS_ALERT_TYPE_FALL);
            g_activity_class = WS_ACT_LYING; /* Reset after alert */
        }

        k_msleep(1000 / BAND_IMU_HZ); /* 20 ms = 50 Hz */
    }
}

/* === WanderNet Task (every 5 min) === */
static void wandernet_task(void *a, void *b, void *c)
{
    while (1) {
        /* Get current hour */
        int64_t now_ms = k_uptime_get();
        uint8_t hour = (uint8_t)((now_ms / 3600000) % 24);

        /* Run WanderNet lite prediction */
        g_wander_risk = wandernet_lite_predict(
            g_gps_lat_e7, g_gps_lon_e7, g_activity_class, hour,
            ws_check_geofence(g_gps_lat_e7, g_gps_lon_e7));

        printk("WanderNet: risk=%d%% activity=%d geofence=%d\n",
               g_wander_risk, g_activity_class,
               ws_check_geofence(g_gps_lat_e7, g_gps_lon_e7));

        /* If risk > critical threshold, send wander alert */
        if (g_wander_risk >= WS_WANDER_RISK_CRITICAL) {
            send_alert(WS_ALERT_TYPE_WANDER);
        }

        k_msleep(BAND_WANDERNET_MS);
    }
}

/* === Telemetry Task (every 5 min) === */
static void telemetry_task(void *a, void *b, void *c)
{
    while (1) {
        /* Read PPG heart rate */
        ppg_read_hr(&g_hr_bpm, &g_hrv_ms);

        /* Send telemetry */
        send_telemetry();

        k_msleep(BAND_TELEM_MS);
    }
}

/* === Radio Task === */
static void radio_task(void *a, void *b, void *c)
{
    while (1) {
        ws_message_t msg;
        int rc = ws_mesh_recv(&g_mesh, &g_radio_hal, &msg, 5000);
        if (rc > 0) {
            switch (msg.header.type) {
            case WS_MSG_JOIN_ACK:
                g_mesh.tdma_slot = msg.payload[0];
                g_mesh.joined = 1;
                printk("Joined mesh, slot %d\n", g_mesh.tdma_slot);
                break;

            case WS_MSG_GEOFENCE_UPD:
                memcpy(&g_geofence, msg.payload, sizeof(ws_geofence_t));
                printk("Geofence updated: radius=%dm\n",
                       g_geofence.radius_m);
                break;

            case WS_MSG_SILENCE:
                printk("Alarm silenced by caregiver\n");
                break;

            case WS_MSG_COMMAND: {
                uint8_t cmd = msg.payload[0];
                if (cmd == WS_CMD_REBOOT) {
                    printk("Reboot command received\n");
                }
                break;
            }
            }
        }

        /* If not joined, attempt to join */
        if (!g_mesh.joined) {
            ws_mesh_join(&g_mesh, &g_radio_hal);
        }
    }
}

/* === Main === */
void main(void)
{
    printk("WanderSync Wander Band starting...\n");

    /* Init mesh */
    uint8_t aes_key[16] = {0};
    ws_mesh_init(&g_mesh, 0x01, WS_NODE_BAND, aes_key);
    g_mesh.battery_v = 420;

    /* Radio HAL */
    g_radio_hal.spi_init   = hal_spi_init;
    g_radio_hal.spi_xfer   = hal_spi_xfer;
    g_radio_hal.cs_low     = hal_cs_low;
    g_radio_hal.cs_high    = hal_cs_high;
    g_radio_hal.reset      = hal_reset;
    g_radio_hal.dio1_read  = hal_dio1_read;
    g_radio_hal.busy_read  = hal_busy_read;
    g_radio_hal.delay_ms  = hal_delay_ms;
    g_radio_hal.delay_us  = hal_delay_us;
    g_radio_hal.on_dio1   = hal_on_dio1;

    g_radio_cfg.freq_hz         = WS_SUBGHZ_FREQ_HZ;
    g_radio_cfg.spreading_factor= WS_SUBGHZ_SF;
    g_radio_cfg.bandwidth_hz    = WS_SUBGHZ_BW_HZ;
    g_radio_cfg.tx_power_dbm    = WS_SUBGHZ_TX_POWER_DBM;
    g_radio_cfg.sync_word       = WS_SYNC_WORD;
    g_radio_cfg.preamble_len    = WS_SUBGHZ_PREAMBLE;

    g_radio_hal.spi_init();
    ws_sx1262_init(&g_radio_hal, &g_radio_cfg);

    /* Init GPS */
    gps_init();

    /* Init I²C for IMU + PPG */
    /* Production: i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0)); */

    /* Init SOS button GPIO with interrupt */
    /* gpio_pin_interrupt_configure(sos_port, BAND_PIN_SOS, GPIO_INT_EDGE_FALLING); */

    /* Init tamper switch GPIO with interrupt */
    /* gpio_pin_interrupt_configure(tamper_port, BAND_PIN_TAMPER, GPIO_INT_EDGE_FALLING); */

    printk("Wander Band ready. Joining mesh...\n");

    /* Start tasks */
    K_THREAD_DEFINE(gps_tid, 2048, gps_task, NULL, NULL, NULL, 5, 0, 0);
    K_THREAD_DEFINE(imu_tid, 2048, imu_task, NULL, NULL, NULL, 6, 0, 0);
    K_THREAD_DEFINE(wn_tid,  2048, wandernet_task, NULL, NULL, NULL, 4, 0, 0);
    K_THREAD_DEFINE(tel_tid, 2048, telemetry_task, NULL, NULL, NULL, 3, 0, 0);
    K_THREAD_DEFINE(rad_tid, 2048, radio_task, NULL, NULL, NULL, 5, 0, 0);

    /* Main loop: low-power wait */
    while (1) {
        k_msleep(60000);
    }
}