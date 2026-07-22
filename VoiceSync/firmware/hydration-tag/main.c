/*
 * VoiceSync — Hydration Tag Firmware (Smart Water Bottle)
 * nRF52840, nRF Connect SDK v2.x
 *
 * The Hydration Tag is a bottle-base ring that measures water mass
 * via a 1 kg load cell + HX711 24-bit ADC, detects sip events via
 * an IMU (LIS2DW12), and transmits cumulative intake to the Hub via
 * BLE 5.0 every 15 minutes. 6-month CR2032 battery life through
 * ultra-low duty cycling (sample every 10s, TX every 15 min).
 *
 * Build: west build -b nrf52840dk_nrf52840
 *
 * Sip detection algorithm:
 *   1. IMU detects bottle lift (acceleration > 0.5g)
 *   2. IMU detects tilt > 30° (drinking position)
 *   3. Load cell confirms mass decrease > 5 g
 *   4. If mass decrease > 5g during lift+tilt → sip event
 *   5. Intake = sum of all mass decreases during sip events
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/sys/printk.h>

#include "../common/protocol.h"
#include "../common/config.h"

#define LOG_TAG "HydrationTag"

/* === HX711 Load Cell === */
#define HX711_RATE_10HZ   0
#define HX711_RATE_80HZ   1

/* Load cell state */
static int32_t g_hx711_offset = 0;    /* Tare offset (zero mass reading) */
static int32_t g_hx711_scale = 2280;  /* Calibration factor (counts per gram) */
static int32_t g_raw_reading = 0;
static int32_t g_water_mass_g = 0;
static int32_t g_last_mass_g = 0;

/* === Sip detection === */
static uint16_t g_sips_24h = 0;
static uint16_t g_intake_ml_24h = 0;
static uint8_t g_last_sip_min = 0;
static uint64_t g_last_sip_timestamp = 0;

/* IMU state */
static float g_accel_x = 0.0f;
static float g_accel_y = 0.0f;
static float g_accel_z = 1.0f;
static uint8_t g_bottle_lifted = 0;
static uint8_t g_bottle_tilted = 0;
static int32_t g_mass_at_lift = 0;

/* === BLE state === */
static uint8_t g_node_id = 0xFF;
static uint16_t g_msg_seq = 0;
static uint8_t g_joined = 0;

/* === HX711 Read ===
 * Reads 24-bit data from HX711 load cell ADC.
 * Protocol: SCK pulses to shift out data.
 *   25-27 pulses for channel/gain selection.
 */
static int32_t hx711_read(void)
{
    /* In production:
     * 1. Wait for DOUT to go low (data ready)
     * 2. Send 25 SCK pulses (Channel A, Gain 128)
     * 3. Read 24 bits from DOUT (MSB first, two's complement)
     * 4. Convert to signed 32-bit
     */
    /* Stub: simulate empty bottle reading */
    return g_hx711_offset + 5000; /* ~500g of water */
}

static void hx711_tare(void)
{
    /* Take 10 readings and average for tare */
    int64_t sum = 0;
    for (int i = 0; i < 10; i++) {
        sum += hx711_read();
        k_msleep(100);
    }
    g_hx711_offset = (int32_t)(sum / 10);
    printk(LOG_TAG ": Tare complete, offset=%d\n", g_hx711_offset);
}

static int32_t hx711_get_mass_g(void)
{
    int32_t raw = hx711_read();
    int32_t mass = (raw - g_hx711_offset) / g_hx711_scale;
    if (mass < 0) mass = 0;
    return mass;
}

/* === IMU Read (LIS2DW12) === */
static void read_imu(void)
{
    /* In production: read LIS2DW12 via I²C at address 0x19
     * Read OUT_X, OUT_Y, OUT_Z registers
     * Convert to g units (±2g range)
     */
    /* Stub: bottle upright, no motion */
    g_accel_x = 0.0f;
    g_accel_y = 0.0f;
    g_accel_z = 1.0f; /* 1g upward */
}

/* === Sip Detection === */
static void detect_sip(void)
{
    /* Total acceleration magnitude */
    float accel_mag = sqrtf(g_accel_x*g_accel_x +
                            g_accel_y*g_accel_y +
                            g_accel_z*g_accel_z);

    /* Detect lift: acceleration spike or pick-up */
    if (accel_mag > 1.5f && !g_bottle_lifted) {
        g_bottle_lifted = 1;
        g_mass_at_lift = g_water_mass_g;
        return;
    }

    /* Detect tilt: bottle tilted for drinking */
    float tilt_angle = atan2f(sqrtf(g_accel_x*g_accel_x + g_accel_y*g_accel_y),
                              fabsf(g_accel_z)) * 180.0f / 3.14159f;
    if (g_bottle_lifted && tilt_angle > 30.0f) {
        g_bottle_tilted = 1;
    }

    /* Detect put-down: acceleration returns to ~1g */
    if (g_bottle_lifted && accel_mag < 1.1f && accel_mag > 0.9f) {
        if (g_bottle_tilted) {
            /* Check mass decrease = sip */
            int32_t mass_after = g_water_mass_g;
            int32_t mass_diff = g_mass_at_lift - mass_after;
            if (mass_diff > 5) {
                /* Sip detected! */
                g_sips_24h++;
                g_intake_ml_24h += (uint16_t)mass_diff; /* 1g ≈ 1mL water */
                g_last_sip_timestamp = k_uptime_get();
                printk(LOG_TAG ": SIP detected! %d g consumed (total: %d mL, %d sips)\n",
                       mass_diff, g_intake_ml_24h, g_sips_24h);
            }
        }
        g_bottle_lifted = 0;
        g_bottle_tilted = 0;
    }
}

/* === Battery Voltage (CR2032) === */
static uint8_t read_battery_mv(void)
{
    /* In production: read ADC on P0.08 (VBAT)
     * CR2032: 3.0V nominal, 2.0V empty
     * Return value in ×0.01V (e.g., 300 = 3.00V)
     */
    return 290; /* Stub: 2.90V */
}

/* === Build and send telemetry via BLE === */
static void send_telemetry(void)
{
    /* Calculate last sip time */
    uint64_t now = k_uptime_get();
    uint32_t since_sip_ms = now - g_last_sip_timestamp;
    g_last_sip_min = (uint8_t)(since_sip_ms / 60000);

    vs_message_t msg;
    vs_build_hydration_telem(&msg, g_node_id, g_msg_seq++,
                             read_battery_mv(),
                             (uint16_t)g_water_mass_g,
                             g_sips_24h,
                             g_intake_ml_24h,
                             g_last_sip_min,
                             0xFF); /* BLE RSSI placeholder */

    /* In production: send via BLE GATT notification */
    uint8_t buf[VS_MAX_MSG];
    size_t len = vs_encode(&msg, buf, sizeof(buf));
    printk(LOG_TAG ": BLE TX %d bytes (mass=%dg, sips=%d, intake=%dmL)\n",
           (int)len, g_water_mass_g, g_sips_24h, g_intake_ml_24h);

    /* Check for dehydration alert */
    if (g_last_sip_min > HYDRATION_REMINDER_MIN) {
        vs_message_t alert;
        vs_build_alert(&alert, g_node_id, g_msg_seq++,
                       VS_ALERT_DEHYDRATION, 1,
                       &g_last_sip_min, 1);
        /* Send via BLE */
    }
}

/* === Sample Task (10-second intervals for ultra-low power) === */
static void sample_task(void *arg)
{
    while (1) {
        /* Read mass */
        g_water_mass_g = hx711_get_mass_g();

        /* Read IMU */
        read_imu();

        /* Sip detection */
        detect_sip();

        printk(LOG_TAG ": mass=%dg sips=%d intake=%dmL\n",
               g_water_mass_g, g_sips_24h, g_intake_ml_24h);

        k_msleep(10000); /* 10 seconds */
    }
}

/* === BLE TX Task (15-minute intervals) === */
static void ble_tx_task(void *arg)
{
    while (1) {
        if (g_joined) {
            send_telemetry();
        }
        k_msleep(HYDRATION_TX_MS); /* 15 minutes */
    }
}

/* === Main === */
int main(void)
{
    printk(LOG_TAG ": VoiceSync Hydration Tag starting...\n");

    /* In production:
     * 1. Initialize HX711 (set rate/gain, tare)
     * 2. Initialize LIS2DW12 IMU via I²C (±2g, 12.5 Hz low-power)
     * 3. Initialize BLE SoftDevice (peripheral, advertise to Hub)
     * 4. Join VoiceSync network via BLE
     * 5. Enter ultra-low-power mode between samples
     */

    /* Tare the load cell */
    hx711_tare();

    /* Get initial mass */
    g_water_mass_g = hx711_get_mass_g();
    g_last_mass_g = g_water_mass_g;

    g_node_id = 3; /* Assigned by Hub in production */
    g_joined = 1;

    /* Start tasks */
    /* In production: use Zephyr thread primitives */
    sample_task(NULL);  /* Runs in main thread (simplified) */

    return 0;
}