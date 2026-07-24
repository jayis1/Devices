/*
 * EchoSync — Wrist Band Firmware
 * nRF52840, nRF Connect SDK, Zephyy RTOS
 *
 * The Wrist Band receives sound event alerts from the Hub via BLE 5.0,
 * delivers distinct haptic vibration patterns based on sound priority,
 * displays the sound type + direction on a 0.96" OLED, detects sleep
 * position via IMU, and reports battery/worn/sleep status to the Hub.
 *
 * Build: west build with nRF Connect SDK v2.x
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/smf.h>

#include "../common/protocol.h"
#include "../common/config.h"

LOG_MODULE_REGISTER(echosync_wrist, LOG_LEVEL_INF);

/* === BLE Service UUIDs ===
 * EchoSync Service: 0000E550-0000-1000-8000-00805F9B34FB
 * Sound Event Char:  0000E551-0000-1000-8000-00805F9B34FB
 * Wrist Status Char:  0000E552-0000-1000-8000-00805F9B34FB
 */

#define ECHOSYNC_SVC_UUID BT_UUID_DECLARE_128( \
    0xE5, 0x50, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, \
    0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB)

#define SOUND_EVENT_UUID BT_UUID_DECLARE_128( \
    0xE5, 0x51, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, \
    0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB)

/* === I2C Devices === */
#define I2C_DEV "I2C_0"
#define SSD1306_ADDR 0x3C
#define DRV2605L_ADDR 0x5A
#define LSM6DS3_ADDR  0x6A

/* === OLED Display (SSD1306 128×64) === */
static const struct device *i2c_dev;
static const struct gpio_dt_spec button_a = GPIO_DT_SPEC_GET(DT_ALIAS(button_a), gpios);
static const struct gpio_dt_spec button_b = GPIO_DT_SPEC_GET(DT_ALIAS(button_b), gpios);

/* Display buffer */
static uint8_t display_buffer[1024]; /* 128×64 / 8 */

static void oled_init(void)
{
    uint8_t init_seq[] = {
        0x00, /* Co=0, D/C=0 (command) */
        0xAE, /* Display off */
        0xD5, 0x80, /* Set display clock divide */
        0xA8, 0x3F, /* Set multiplex */
        0xD3, 0x00, /* Set display offset */
        0x40, /* Set start line */
        0x8D, 0x14, /* Enable charge pump */
        0x20, 0x00, /* Memory addressing mode: horizontal */
        0xA1, /* Segment remap */
        0xC8, /* COM scan direction */
        0xDA, 0x12, /* COM pins */
        0x81, 0xCF, /* Set contrast */
        0xD9, 0xF1, /* Set precharge */
        0xDB, 0x40, /* Set VCOMH */
        0xA4, /* Display from RAM */
        0xA6, /* Normal display */
        0xAF, /* Display on */
    };
    i2c_write(i2c_dev, init_seq, sizeof(init_seq), SSD1306_ADDR);
}

static void oled_clear(void)
{
    memset(display_buffer, 0, sizeof(display_buffer));
}

static void oled_set_pixel(int x, int y, int on)
{
    if (x < 0 || x >= 128 || y < 0 || y >= 64) return;
    int idx = x + (y / 8) * 128;
    if (on) display_buffer[idx] |= (1 << (y % 8));
    else display_buffer[idx] &= ~(1 << (y % 8));
}

static void oled_draw_text(int x, int y, const char *text, int size)
{
    /* Simplified: in production use a font library */
    (void)x; (void)y; (void)text; (void)size;
}

static void oled_flush(void)
{
    uint8_t cmd[] = {0x00, 0x21, 0x00, 0x7F, 0x22, 0x00, 0x07};
    i2c_write(i2c_dev, cmd, sizeof(cmd), SSD1306_ADDR);
    for (int i = 0; i < 1024; i += 16) {
        uint8_t buf[17];
        buf[0] = 0x40; /* Data */
        memcpy(&buf[1], &display_buffer[i], 16);
        i2c_write(i2c_dev, buf, 17, SSD1306_ADDR);
    }
}

/* === Haptic Driver (DRV2605L) === */
static void haptic_init(void)
{
    /* Reset device */
    uint8_t mode = 0x07; /* Reset */
    i2c_write(i2c_dev, &mode, 1, DRV2605L_ADDR);
    k_msleep(10);

    /* Set mode: internal trigger */
    uint8_t data[][2] = {
        {0x01, 0x80 | 0x20 | 0x01}, /* Mode: internal trigger, ERM motor */
        {0x1D, 0x00}, /* Library: ERM */
        {0x1E, 0x40}, /* Closed loop, ERM */
    };
    for (int i = 0; i < 3; i++)
        i2c_write(i2c_dev, data[i], 2, DRV2605L_ADDR);
}

static void haptic_play(uint8_t effect_id)
{
    /* Set waveform sequence (slot 0 = effect, slot 1 = end) */
    uint8_t seq[] = {0x04, effect_id, 0x05, 0x00};
    uint8_t buf[2];
    buf[0] = 0x04; buf[1] = effect_id;
    i2c_write(i2c_dev, buf, 2, DRV2605L_ADDR);
    buf[0] = 0x05; buf[1] = 0x00;
    i2c_write(i2c_dev, buf, 2, DRV2605L_ADDR);

    /* Go */
    uint8_t go[] = {0x0C, 0x01};
    i2c_write(i2c_dev, go, 2, DRV2605L_ADDR);
}

static void haptic_alert(uint8_t priority)
{
    switch (priority) {
        case ES_PRIORITY_EMERGENCY:
            /* Triple-burst: effect 73 (Sharp Click 100%) ×3 */
            haptic_play(73);
            k_msleep(150);
            haptic_play(73);
            k_msleep(150);
            haptic_play(73);
            break;
        case ES_PRIORITY_IMPORTANT:
            /* Double-pulse: effect 47 (Double Click 100%) */
            haptic_play(47);
            break;
        case ES_PRIORITY_INFO:
            /* Single-tap: effect 12 (Soft Bump 60%) */
            haptic_play(12);
            break;
    }
}

/* === IMU (LSM6DS3TR-C) — Sleep Detection === */
static uint8_t g_sleeping = 0;
static uint8_t g_worn = 1;

static void imu_init(void)
{
    uint8_t cfg[][2] = {
        {0x10, 0x20}, /* CTRL1_XL: 104 Hz, ±2g */
        {0x11, 0x20}, /* CTRL2_G: 104 Hz, ±250 dps */
        {0x12, 0x44}, /* CTRL3_C: BDU, IF_INC */
        {0x0D, 0x01}, /* INT1_CTRL: XL interrupt */
    };
    for (int i = 0; i < 4; i++)
        i2c_write(i2c_dev, cfg[i], 2, LSM6DS3_ADDR);
}

static void imu_read_accel(int16_t *ax, int16_t *ay, int16_t *az)
{
    uint8_t reg = 0x28;
    uint8_t buf[6];
    /* i2c_write_read: read 6 bytes from accel registers */
    i2c_write(i2c_dev, &reg, 1, LSM6DS3_ADDR);
    i2c_read(i2c_dev, buf, 6, LSM6DS3_ADDR);
    *ax = (int16_t)(buf[0] | (buf[1] << 8));
    *ay = (int16_t)(buf[2] | (buf[3] << 8));
    *az = (int16_t)(buf[4] | (buf[5] << 8));
}

static void detect_sleep(void)
{
    int16_t ax, ay, az;
    imu_read_accel(&ax, &ay, &az);
    float mag = sqrtf((float)ax * ax + (float)ay * ay + (float)az * az);

    /* If acceleration is mainly gravity (~1g = ~16384 at ±2g range)
     * and oriented like wrist-down (ay dominant), assume sleeping */
    if (az > 14000 && abs(ay) < 4000) {
        g_sleeping = 1;
    } else if (mag > 5000) {
        g_sleeping = 0;
        g_worn = 1;
    }
}

/* === Battery Voltage === */
static uint16_t read_battery_mv(void)
{
    /* ADC read on VBAT pin (P0.10) */
    /* In production: use Zephyr ADC driver */
    /* Simulated */
    return 3700;
}

/* === Sound Event Display === */
static const char *sound_icons[20] = {
    "🔥", "⚠️", "💥", "🚨", "🔔", "✊", "📞", "👶",
    "🚗", "🚪", "🚪", "💧", "🐕", "⏰", "🍲", "🍽️",
    "🌀", "🚶", "★1", "★2"
};

static void display_sound_event(uint8_t sound_class, uint8_t priority,
                                 uint16_t direction, uint8_t source_node)
{
    oled_clear();
    oled_draw_text(0, 0, "ECHO ALERT", 1);

    /* Sound icon */
    if (sound_class < 20)
        oled_draw_text(0, 16, sound_icons[sound_class], 2);

    /* Direction arrow */
    int dir = direction / 100; /* 0-35 */
    char dir_str[16];
    snprintf(dir_str, sizeof(dir_str), "Dir: %d°", direction / 10);
    oled_draw_text(64, 16, dir_str, 1);

    /* Priority */
    const char *pri_text;
    switch (priority) {
        case ES_PRIORITY_EMERGENCY: pri_text = "EMERGENCY"; break;
        case ES_PRIORITY_IMPORTANT: pri_text = "IMPORTANT"; break;
        default: pri_text = "Info"; break;
    }
    oled_draw_text(0, 48, pri_text, 1);

    oled_flush();
}

/* === BLE GATT: Sound Event Characteristic === */
static uint8_t sound_event_data[12];
static ssize_t sound_event_read(struct bt_conn *conn,
                                 const struct bt_gatt_attr *attr,
                                 void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                             sound_event_data, sizeof(sound_event_data));
}

static ssize_t sound_event_write(struct bt_conn *conn,
                                  const struct bt_gatt_attr *attr,
                                  const void *buf, uint16_t len,
                                  uint16_t offset, uint8_t flags)
{
    if (len < 12) return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);

    uint8_t sound_class = ((const uint8_t *)buf)[0];
    uint8_t priority = ((const uint8_t *)buf)[1];
    uint8_t confidence = ((const uint8_t *)buf)[2];
    uint16_t direction = ((const uint8_t *)buf)[3] | (((const uint8_t *)buf)[4] << 8);
    uint8_t source_node = ((const uint8_t *)buf)[5];

    LOG_INF("BLE Sound Event: class=%d pri=%d conf=%d dir=%d node=%d",
            sound_class, priority, confidence, direction, source_node);

    /* Check if we should alert (suppress non-emergency during sleep) */
    if (g_sleeping && priority < ES_PRIORITY_EMERGENCY) {
        LOG_INF("Suppressed (sleeping, non-emergency)");
        return len;
    }

    /* Haptic alert */
    haptic_alert(priority);

    /* Display update */
    display_sound_event(sound_class, priority, direction, source_node);

    return len;
}

/* BLE Service definition */
BT_GATT_SERVICE_DEFINE(echosync_svc,
    BT_GATT_PRIMARY_SERVICE(ECHOSYNC_SVC_UUID),
    BT_GATT_CHARACTERISTIC(SOUND_EVENT_UUID,
        BT_GATT_CHRC_WRITE | BT_GATT_CHRC_READ,
        BT_GATT_PERM_WRITE | BT_GATT_PERM_READ,
        sound_event_read, sound_event_write,
        sound_event_data),
);

/* BLE advertising data */
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL,
        0xE5, 0x50, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
        0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB),
};

static void bt_ready(int err)
{
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        return;
    }
    LOG_INF("Bluetooth initialized");
    bt_le_adv_start(BT_LE_ADV_CONN_DEFAULT, ad, ARRAY_SIZE(ad), NULL, 0);
}

/* === Buttons === */
static struct gpio_callback button_a_cb;
static struct gpio_callback button_b_cb;
static uint8_t alerts_silenced = 0;

static void button_a_pressed(const struct device *port,
                             struct gpio_callback *cb, uint32_t pins)
{
    LOG_INF("Button A: Acknowledge/Dismiss alert");
    /* Dismiss current alert */
    alerts_silenced = 1;
}

static void button_b_pressed(const struct device *port,
                             struct gpio_callback *cb, uint32_t pins)
{
    LOG_INF("Button B: View next event / menu");
}

/* === Main === */
int main(void)
{
    LOG_INF("EchoSync Wrist Band starting...");

    i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
    if (!device_is_ready(i2c_dev)) {
        LOG_ERR("I2C not ready");
        return -1;
    }

    /* Init peripherals */
    oled_init();
    haptic_init();
    imu_init();

    /* Init buttons */
    gpio_pin_configure_dt(&button_a, GPIO_INPUT);
    gpio_pin_configure_dt(&button_b, GPIO_INPUT);
    gpio_init_callback(&button_a_cb, button_a_pressed, BIT(button_a.pin));
    gpio_init_callback(&button_b_cb, button_b_pressed, BIT(button_b.pin));
    gpio_add_callback(button_a.port, &button_a_cb);
    gpio_add_callback(button_b.port, &button_b_cb);

    /* Init BLE */
    int err = bt_enable(bt_ready);
    if (err) {
        LOG_ERR("Bluetooth init failed");
        return -1;
    }

    LOG_INF("EchoSync Wrist Band ready");

    /* Main loop: periodic sleep detection + battery + telemetry */
    uint16_t alerts_24h = 0;
    uint8_t last_alert_class = 0xFF;

    while (1) {
        detect_sleep();

        /* Check battery */
        uint16_t batt_mv = read_battery_mv();
        if (batt_mv < WB_BATTERY_LOW_MV) {
            LOG_WRN("Battery low: %dmV", batt_mv);
        }

        /* In production: send telemetry via BLE to hub */
        LOG_INF("Status: batt=%dmV worn=%d sleeping=%d alerts=%d",
                batt_mv, g_worn, g_sleeping, alerts_24h);

        k_sleep(K_SECONDS(30));
    }

    return 0;
}