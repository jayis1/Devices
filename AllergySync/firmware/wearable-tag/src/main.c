/*
 * AllergySync — Wearable Tag Firmware (nRF52840, Zephyr RTOS)
 *
 * Coin-cell wearable for personal pollen exposure tracking:
 *  - Plantower PMSA003I mini PM sensor (UART, duty-cycled)
 *  - BMI270 IMU for activity classification (TinyCNN)
 *  - LR1121 Sub-GHz for mesh (when in range of hub)
 *  - BLE 5.0 GATT for phone (when outdoors)
 *  - CR2032 coin cell, 9-month battery life
 *  - Deep sleep (3 µA system current between samples)
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/rand32.h>
#include <zephyr/pm/pm.h>

#include "common/allergysync_proto.h"
#include "common/as_lr1121.h"
#include "common/as_tdma.h"

LOG_MODULE_REGISTER(wearable_tag, LOG_LEVEL_INF);

/* ---- Pin definitions (nRF52840) ---- */
#define PIN_LR_CS       4
#define PIN_LR_SCLK     5
#define PIN_LR_MOSI     6
#define PIN_LR_MISO     7
#define PIN_LR_DIO0     8
#define PIN_LR_DIO1     9
#define PIN_LR_BUSY     10
#define PIN_LR_RESET    11

#define PIN_PMS_TX      15
#define PIN_PMS_RX      16
#define PIN_PMS_EN      17

#define PIN_IMU_SDA     19
#define PIN_IMU_SCL     20
#define PIN_IMU_INT     21

#define PIN_LED         23
#define PIN_BUTTON      25

/* ---- Timing constants ---- */
#define PM_SAMPLE_INTERVAL_S    300   /* 5 minutes */
#define PM_SAMPLE_DURATION_MS   8000  /* 8 seconds active */
#define BLE_ADVERT_INTERVAL_MS   1000
#define MESH_TX_INTERVAL_S       600  /* 10 minutes */

/* ---- PMSA003I data frame ---- */
typedef struct {
    uint16_t pm1_0_std;
    uint16_t pm2_5_std;
    uint16_t pm10_std;
    uint16_t pm1_0_env;
    uint16_t pm2_5_env;
    uint16_t pm10_env;
    uint16_t count_03;
    uint16_t count_05;
    uint16_t count_10;
    uint16_t count_25;
    uint16_t count_50;
    uint16_t count_100;
} pms_data_t;

static pms_data_t pms_latest;

/* ---- Activity classifier (TinyCNN, stub) ---- */
typedef enum { ACT_STATIC, ACT_WALKING, ACT_RUNNING } act_t;

static act_t classify_activity(const int16_t *accel, int len)
{
    /* TinyCNN inference would happen here (tflite-micro) */
    /* Stub: compute magnitude variance as simple heuristic */
    float mean = 0, var = 0;
    for (int i = 0; i < len; i++) mean += accel[i];
    mean /= len;
    for (int i = 0; i < len; i++) var += (accel[i] - mean) * (accel[i] - mean);
    var /= len;

    if (var < 100) return ACT_STATIC;
    if (var < 1000) return ACT_WALKING;
    return ACT_RUNNING;
}

/* ---- TDMA context ---- */
static as_tdma_node_t tdma;

/* ---- Platform port for LR1121 ---- */
static const struct device *spi_dev;
static const struct device *i2c_dev;
static const struct device *gpio_dev;

static void lr_cs_select(void) { gpio_pin_set(gpio_dev, PIN_LR_CS, 0); }
static void lr_cs_release(void) { gpio_pin_set(gpio_dev, PIN_LR_CS, 1); }
static void lr_spi_xfer(const uint8_t *tx, uint8_t *rx, size_t len)
{
    const struct spi_config cfg = {
        .frequency = 8000000,
        .operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB,
    };
    struct spi_buf tx_buf = { .buf = (void *)tx, .len = len };
    struct spi_buf_set tx_set = { .buffers = &tx_buf, .count = 1 };
    struct spi_buf rx_buf = { .buf = rx, .len = len };
    struct spi_buf_set rx_set = { .buffers = &rx_buf, .count = 1 };
    gpio_pin_set(gpio_dev, PIN_LR_CS, 0);
    spi_transceive(spi_dev, &cfg, &tx_set, &rx_set);
    gpio_pin_set(gpio_dev, PIN_LR_CS, 1);
}
static void lr_reset(bool assert) { gpio_pin_set(gpio_dev, PIN_LR_RESET, assert ? 0 : 1); }
static bool lr_busy_read(void) { return gpio_pin_get(gpio_dev, PIN_LR_BUSY) != 0; }
static void lr_delay_ms(uint32_t ms) { k_msleep(ms); }

static as_lr1121_port_t lr_port = {
    .cs_select  = lr_cs_select,
    .cs_release = lr_cs_release,
    .spi_xfer   = lr_spi_xfer,
    .reset      = lr_reset,
    .busy_read  = lr_busy_read,
    .delay_ms   = lr_delay_ms,
};

/* ---- PMSA003I driver (UART) ---- */
static const struct device *uart_dev;

static void pms_enable(bool on)
{
    gpio_pin_set(gpio_dev, PIN_PMS_EN, on ? 1 : 0);
}

static int pms_read(pms_data_t *data)
{
    uint8_t rx_buf[32];
    int total = 0;
    uint32_t start = k_uptime_get_32();

    /* Wait for PMSA003I to output frame (active output, 1s interval) */
    while (total < 32 && (k_uptime_get_32() - start) < PM_SAMPLE_DURATION_MS) {
        uint8_t byte;
        int n = uart_fifo_read(uart_dev, &byte, 1);
        if (n > 0 && total < 32)
            rx_buf[total++] = byte;
    }

    if (total < 32)
        return -1;

    /* Check frame header: 0x42 0x4D */
    if (rx_buf[0] != 0x42 || rx_buf[1] != 0x4D)
        return -1;

    /* Parse (big-endian, standard PMSA003I frame) */
    data->pm1_0_std  = (rx_buf[4]  << 8) | rx_buf[5];
    data->pm2_5_std  = (rx_buf[6]  << 8) | rx_buf[7];
    data->pm10_std   = (rx_buf[8]  << 8) | rx_buf[9];
    data->pm1_0_env  = (rx_buf[10] << 8) | rx_buf[11];
    data->pm2_5_env  = (rx_buf[12] << 8) | rx_buf[13];
    data->pm10_env   = (rx_buf[14] << 8) | rx_buf[15];
    data->count_03   = (rx_buf[16] << 8) | rx_buf[17];
    data->count_05   = (rx_buf[18] << 8) | rx_buf[19];
    data->count_10   = (rx_buf[20] << 8) | rx_buf[21];
    data->count_25   = (rx_buf[22] << 8) | rx_buf[23];
    data->count_50   = (rx_buf[24] << 8) | rx_buf[25];
    data->count_100  = (rx_buf[26] << 8) | rx_buf[27];

    return 0;
}

/* ---- BLE GATT (for phone app when outdoors) ---- */
#define BLE_SERVICE_UUID  0x180A  /* Device Information (stub) */
#define BLE_CHAR_UUID     0x2A58  /* AllergenSync exposure data */

static uint8_t ble_exposure_data[24];
static ssize_t ble_read_exposure(struct bt_conn *conn,
                                  const struct bt_gatt_attr *attr,
                                  void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                             ble_exposure_data, sizeof(ble_exposure_data));
}

static struct bt_gatt_attr attrs[] = {
    BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_16(BLE_SERVICE_UUID)),
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(BLE_CHAR_UUID),
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ, ble_read_exposure, NULL,
                           ble_exposure_data),
};

static struct bt_gatt_service svc = BT_GATT_SERVICE(attrs);

static void ble_init(void)
{
    bt_enable(NULL);
    bt_gatt_service_register(&svc);

    /* Start advertising */
    struct bt_le_adv_param param = {
        .options = BT_LE_ADV_OPT_CONNECTABLE,
        .interval_min = 0x0020, /* 20 ms */
        .interval_max = 0x0040,
    };
    const char *name = "AllergySync Tag";
    bt_le_adv_start(&param, &name, 1, NULL, 0);
    LOG_INF("BLE advertising started");
}

/* ---- BMI270 IMU (I2C) ---- */
static void imu_read(int16_t *accel, int16_t *gyro)
{
    uint8_t reg = 0x0C; /* Accel data register */
    uint8_t data[12];
    i2c_write_read(i2c_dev, 0x68, &reg, 1, data, 12);
    for (int i = 0; i < 3; i++) {
        accel[i] = (data[i * 2] << 8) | data[i * 2 + 1];
        gyro[i]  = (data[6 + i * 2] << 8) | data[6 + i * 2 + 1];
    }
}

/* ---- Battery monitoring (ADC on CR2032) ---- */
static uint16_t read_battery_mv(void)
{
    /* nRF52840 SAADC, VDD divider */
    /* Stub: real implementation uses nrfx_saadc */
    return 3000; /* ~3.0V = fresh CR2032 */
}

static uint8_t battery_pct(uint16_t mv)
{
    /* CR2032: 3.0V fresh, 2.0V empty */
    if (mv >= 3000) return 100;
    if (mv <= 2000) return 0;
    return (uint8_t)(100 * (mv - 2000) / 1000);
}

/* ---- Sampling + TX task ---- */
static void sample_and_tx(void)
{
    LOG_INF("Sampling...");

    /* Enable PMS sensor */
    pms_enable(true);
    k_msleep(8000); /* Warm-up: 8s for stable readings */

    /* Read PM */
    if (pms_read(&pms_latest) == 0) {
        LOG_INF("PM2.5=%d PM10=%d", pms_latest.pm2_5_env, pms_latest.pm10_env);
    } else {
        LOG_WRN("PMS read failed");
        memset(&pms_latest, 0, sizeof(pms_latest));
    }

    /* Disable PMS sensor to save power */
    pms_enable(false);

    /* Read IMU + classify activity */
    int16_t accel[3], gyro[3];
    imu_read(accel, gyro);
    act_t activity = classify_activity(accel, 0);

    /* Read battery */
    uint16_t batt_mv = read_battery_mv();
    uint8_t batt_pct = battery_pct(batt_mv);

    /* Build telemetry */
    as_telem_wearable_t telem;
    memset(&telem, 0, sizeof(telem));
    telem.pm2_5 = pms_latest.pm2_5_env * 10;
    telem.pm10  = pms_latest.pm10_env * 10;
    telem.pollen_class = AS_POLLEN_NONE; /* Could run PollenNet here too */
    telem.activity = (uint8_t)activity;
    telem.battery_mv = batt_mv;
    telem.battery_pct = batt_pct;
    telem.exposure_idx = 0; /* Incremented across samples */

    /* Update BLE exposure data */
    memcpy(ble_exposure_data, &telem, sizeof(telem));

    /* Try to send via Sub-GHz mesh */
    if (tdma.synced) {
        as_tdma_send(&tdma, AS_MSG_TELEMETRY, 0,
                     (uint8_t *)&telem, sizeof(telem));
        LOG_INF("Telemetry sent via mesh");
    }

    LOG_INF("Sample done: PM2.5=%d act=%d batt=%d%%",
            telem.pm2_5, activity, batt_pct);
}

/* ---- Main ---- */
int main(void)
{
    LOG_INF("AllergySync Wearable Tag starting (nRF52840)...");

    /* Get devices */
    spi_dev = DEVICE_DT_GET(DT_NODELABEL(spi0));
    i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
    gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
    uart_dev = DEVICE_DT_GET(DT_NODELABEL(uart0));

    if (!device_is_ready(spi_dev) || !device_is_ready(i2c_dev) ||
        !device_is_ready(gpio_dev) || !device_is_ready(uart_dev)) {
        LOG_ERR("Device not ready");
        return -1;
    }

    /* Configure GPIO */
    gpio_pin_configure(gpio_dev, PIN_LR_CS, GPIO_OUTPUT_HIGH);
    gpio_pin_configure(gpio_dev, PIN_LR_RESET, GPIO_OUTPUT_HIGH);
    gpio_pin_configure(gpio_dev, PIN_LR_BUSY, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_configure(gpio_dev, PIN_LR_DIO0, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_configure(gpio_dev, PIN_LR_DIO1, GPIO_INPUT | GPIO_PULL_UP);

    gpio_pin_configure(gpio_dev, PIN_PMS_EN, GPIO_OUTPUT_LOW);
    gpio_pin_configure(gpio_dev, PIN_LED, GPIO_OUTPUT_LOW);
    gpio_pin_configure(gpio_dev, PIN_BUTTON, GPIO_INPUT | GPIO_PULL_UP);

    /* Init LR1121 */
    if (as_lr1121_init(&lr_port) != 0) {
        LOG_ERR("LR1121 init failed!");
    } else {
        as_lr1121_set_channel(868100000);
        as_lr1121_set_tx_power(10); /* Lower power for wearable */
        as_lr1121_set_modem_fsk(50000, 25000, 100000);
        uint8_t sync[] = { 0xA5, 0x1E, 0x9C, 0x47 };
        as_lr1121_set_sync_word(sync, 4);

        as_tdma_init(&tdma, false, &lr_port);
        uint8_t pubkey[64] = {0};
        as_tdma_join(&tdma, AS_NODE_WEARABLE, pubkey);
    }

    /* Init BLE */
    ble_init();

    LOG_INF("Wearable Tag running. Sampling every %d seconds.",
            PM_SAMPLE_INTERVAL_S);

    /* Main loop: sample → sleep → repeat */
    while (1) {
        sample_and_tx();

        /* Deep sleep until next sample */
        as_lr1121_sleep();

        /* Sleep for 5 minutes (in 30s chunks to allow BLE) */
        for (int i = 0; i < 10; i++) {
            k_sleep(K_SECONDS(30));
        }

        as_lr1121_standby();
    }

    return 0;
}