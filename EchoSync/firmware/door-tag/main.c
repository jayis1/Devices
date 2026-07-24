/*
 * EchoSync — Door Tag Firmware
 * nRF52840, nRF Connect SDK, Zephyr RTOS
 *
 * The Door Tag detects doorbell, door knock, and phone ring at the source.
 * It uses a piezo contact sensor for physical vibration detection (knock,
 * doorbell mechanism) and an I²S MEMS microphone for ring-tone detection.
 * Ultra-low-power: 12-month CR2032 battery life through duty-cycling.
 *
 * Build: west build with nRF Connect SDK v2.x
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/util.h>
#include <zephyr/random/rand.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include "../common/protocol.h"
#include "../common/config.h"

LOG_MODULE_REGISTER(echosync_door, LOG_LEVEL_INF);

/* === Door Tag BLE Service === */
#define DOOR_SVC_UUID BT_UUID_DECLARE_128( \
    0xE5, 0x60, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, \
    0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB)

#define DOOR_EVENT_UUID BT_UUID_DECLARE_128( \
    0xE5, 0x61, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, \
    0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB)

/* === I2C / ADC devices === */
static const struct device *i2c_dev;
static const struct adc_dt_spec piezo_adc =
    ADC_DT_SPEC_GET(DT_PATH(adc, piezo));

/* === Knock Detection State === */
static struct {
    uint32_t last_knock_time;
    uint8_t knock_count;
    uint32_t window_start;
} knock_state;

static uint16_t g_event_counter = 0;
static uint8_t g_connected = 0;
static struct bt_conn *g_conn = NULL;

/* === Piezo ADC Sampling === */
static int16_t sample_piezo(void)
{
    int16_t val = 0;
    int ret = adc_read(piezo_adc.dev, &(struct adc_sequence){
        .channels = BIT(piezo_adc.channel_id),
        .buffer = &val,
        .buffer_size = sizeof(val),
        .resolution = 12,
        .oversampling = 4,
    });
    if (ret) {
        LOG_ERR("ADC read failed: %d", ret);
        return 0;
    }
    return val;
}

/* === Knock Detection Algorithm === */
static int detect_knock(int16_t piezo_val)
{
    uint32_t now = k_uptime_get_32();

    if (piezo_val > DT_PIEZO_THRESHOLD) {
        if (knock_state.window_start == 0) {
            knock_state.window_start = now;
            knock_state.knock_count = 1;
            LOG_INF("Knock 1 detected (val=%d)", piezo_val);
        } else if (now - knock_state.last_knock_time < DT_KNOCK_WINDOW_MS) {
            knock_state.knock_count++;
            LOG_INF("Knock %d detected (val=%d)", knock_state.knock_count, piezo_val);
        }

        knock_state.last_knock_time = now;
        return 1;
    }

    /* Check if knock window expired */
    if (knock_state.window_start > 0 &&
        (now - knock_state.window_start) > DT_KNOCK_WINDOW_MS) {
        if (knock_state.knock_count >= 2) {
            LOG_INF("Knock pattern complete: %d knocks", knock_state.knock_count);
            int count = knock_state.knock_count;
            knock_state.window_start = 0;
            knock_state.knock_count = 0;
            return count;
        }
        knock_state.window_start = 0;
        knock_state.knock_count = 0;
    }

    return 0;
}

/* === I²S MEMS Microphone (simplified ring-tone detection) === */
static void mic_power_on(void)
{
    /* Enable MOSFET gate for microphone power */
    const struct gpio_dt_spec mic_en = GPIO_DT_SPEC_GET(DT_ALIAS(mic_en), gpios);
    gpio_pin_set_dt(&mic_en, 1);
    k_msleep(10); /* Warm-up */
}

static void mic_power_off(void)
{
    const struct gpio_dt_spec mic_en = GPIO_DT_SPEC_GET(DT_ALIAS(mic_en), gpios);
    gpio_pin_set_dt(&mic_en, 0);
}

static uint8_t detect_ringtone(void)
{
    /* In production: read 2 seconds of I²S audio, run ring-tone classifier */
    /* Simplified: check for tonal pattern in audio */
    /* Returns: 0=none, 1=doorbell, 2=phone, 3=custom */
    return 0; /* No ring-tone detected in this simplified version */
}

/* === BLE GATT: Door Event Characteristic === */
static uint8_t door_event_data[8];

static ssize_t door_event_read(struct bt_conn *conn,
                                const struct bt_gatt_attr *attr,
                                void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                             door_event_data, sizeof(door_event_data));
}

static ssize_t door_event_write(struct bt_conn *conn,
                                 const struct bt_gatt_attr *attr,
                                 const void *buf, uint16_t len,
                                 uint16_t offset, uint8_t flags)
{
    return len;
}

BT_GATT_SERVICE_DEFINE(door_svc,
    BT_GATT_PRIMARY_SERVICE(DOOR_SVC_UUID),
    BT_GATT_CHARACTERISTIC(DOOR_EVENT_UUID,
        BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
        BT_GATT_PERM_READ,
        door_event_read, door_event_write,
        door_event_data),
);

/* Send door event via BLE notification */
static void send_door_event(uint8_t event_type, uint8_t confidence,
                            uint8_t knock_count)
{
    g_event_counter++;
    door_event_data[0] = event_type;
    door_event_data[1] = confidence;
    door_event_data[2] = knock_count;
    door_event_data[3] = (uint8_t)(g_event_counter & 0xFF);
    door_event_data[4] = (uint8_t)(g_event_counter >> 8);
    door_event_data[5] = 0; /* reserved */
    door_event_data[6] = 0;
    door_event_data[7] = 0;

    if (g_conn) {
        bt_gatt_notify(g_conn, &door_svc.attrs[2],
                       door_event_data, sizeof(door_event_data));
    }
    LOG_INF("Door event sent: type=%d conf=%d knocks=%d id=%d",
            event_type, confidence, knock_count, g_event_counter);
}

/* BLE connection callbacks */
static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        LOG_ERR("BLE connection failed (err %u)", err);
        return;
    }
    g_conn = bt_conn_ref(conn);
    g_connected = 1;
    LOG_INF("BLE connected");
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("BLE disconnected (reason %u)", reason);
    if (g_conn) {
        bt_conn_unref(g_conn);
        g_conn = NULL;
    }
    g_connected = 0;
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

/* BLE advertising */
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL,
        0xE5, 0x60, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
        0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB),
};

/* === Battery === */
static uint16_t read_battery_mv(void)
{
    /* ADC on VBAT pin (P0.07) */
    /* In production: use Zephyr ADC driver */
    return 3000; /* Simulated */
}

/* === Main === */
int main(void)
{
    LOG_INF("EchoSync Door Tag starting...");

    /* Init ADC for piezo */
    if (!device_is_ready(piezo_adc.dev)) {
        LOG_ERR("ADC not ready");
        return -1;
    }
    adc_channel_setup_dt(&piezo_adc);

    /* Init GPIO for mic enable */
    const struct gpio_dt_spec mic_en = GPIO_DT_SPEC_GET(DT_ALIAS(mic_en), gpios);
    gpio_pin_configure_dt(&mic_en, GPIO_OUTPUT);
    mic_power_off();

    /* Init BLE */
    int err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth init failed");
        return -1;
    }
    bt_le_adv_start(BT_LE_ADV_CONN_DEFAULT, ad, ARRAY_SIZE(ad), NULL, 0);
    LOG_INF("BLE advertising started");

    /* Main loop: ultra-low-power knock detection */
    uint32_t last_mic_listen = k_uptime_get_32();

    while (1) {
        /* Sample piezo sensor continuously (low-power ADC) */
        int16_t piezo = sample_piezo();
        int knock_result = detect_knock(piezo);

        if (knock_result >= 2) {
            /* Multiple knocks detected — send door knock event */
            send_door_event(0 /* knock */, 95 /* confidence */,
                           (uint8_t)knock_result);
        }

        /* Periodic mic listen for ring-tone detection (every 30s) */
        uint32_t now = k_uptime_get_32();
        if (now - last_mic_listen > (DT_MIC_LISTEN_INTERVAL_S * 1000)) {
            mic_power_on();
            uint8_t ring = detect_ringtone();
            if (ring > 0) {
                send_door_event(ring, 88, 0);
            }
            mic_power_off();
            last_mic_listen = now;
        }

        /* Check battery */
        static uint32_t last_batt_check = 0;
        if (now - last_batt_check > 3600000) { /* hourly */
            uint16_t batt = read_battery_mv();
            if (batt < DT_BATTERY_LOW_MV) {
                LOG_WRN("Battery low: %dmV", batt);
                /* Send low battery alert */
            }
            last_batt_check = now;
        }

        /* Ultra-low-power sleep between samples */
        k_sleep(K_MSEC(10)); /* 100 Hz sampling */
    }

    return 0;
}