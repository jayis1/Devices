/*
 * GrillSync — Meat Probe Firmware
 * nRF52840, nRF Connect SDK (Zephyr RTOS)
 *
 * The Meat Probe uses 4× MAX31855K Type-K thermocouple interfaces
 * to measure meat internal temperature at 4 depths. Communicates
 * with the Grill Hub via BLE 5.0. Reports every 2s during active cook.
 *
 * Build: west build -b nrf52840dk_nrf52840
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>
#include <math.h>

#include "../common/protocol.h"
#include "../common/config.h"

LOG_MODULE_REGISTER(grillsync_probe, LOG_LEVEL_INF);

/* === SPI Configuration for MAX31855 === */
#define SPI_DEV_ID DT_NODELABEL(spi0)
#define MAX31855_CS1_NODE DT_ALIAS(tc1_cs)
#define MAX31855_CS2_NODE DT_ALIAS(tc2_cs)
#define MAX31855_CS3_NODE DT_ALIAS(tc3_cs)
#define MAX31855_CS4_NODE DT_ALIAS(tc4_cs)

static const struct device *spi_dev;
static const struct device *gpio_dev;

static struct gpio_dt_spec tc_cs[4] = {
    GPIO_DT_SPEC_GET(MAX31855_CS1_NODE, gpios),
    GPIO_DT_SPEC_GET(MAX31855_CS2_NODE, gpios),
    GPIO_DT_SPEC_GET(MAX31855_CS3_NODE, gpios),
    GPIO_DT_SPEC_GET(MAX31855_CS4_NODE, gpios),
};

/* === MAX31855 Thermocouple Reader === */
/*
 * MAX31855K SPI protocol:
 * - 32-bit read: [D31:D18] thermocouple temp (14-bit signed, 0.25°C/bit)
 *               [D15:D4]  internal temp (12-bit signed, 0.0625°C/bit)
 *               [D3]      reserved
 *               [D2]      fault (1=fault)
 *               [D1]      open circuit
 *               [D0]      GND short
 */
static int16_t read_max31855(int probe_idx, int16_t *internal_temp, uint8_t *fault)
{
    uint8_t rx[4] = {0};

    /* Assert CS */
    gpio_pin_set_dt(&tc_cs[probe_idx], 0);
    k_usleep(1);

    struct spi_buf rx_buf = { .buf = rx, .len = 4 };
    struct spi_buf_set rx_set = { .buffers = &rx_buf, .count = 1 };

    /* Read 32 bits via SPI */
    /* In production: spi_transceive(spi_dev, &spi_cfg, NULL, &rx_set) */
    /* Placeholder: simulate reading */
    rx[0] = 0x01;  /* ~4°C sim */
    rx[1] = 0x00;
    rx[2] = 0x01;
    rx[3] = 0x00;

    k_usleep(1);
    gpio_pin_set_dt(&tc_cs[probe_idx], 1);

    /* Parse 14-bit signed thermocouple temperature */
    int16_t raw_tc = ((int16_t)rx[0] << 6) | (rx[1] >> 2);
    if (raw_tc & 0x2000) {  /* Sign extend */
        raw_tc |= 0xC000;
    }
    int16_t tc_deci = raw_tc * 25 / 10;  /* 0.25°C → ×0.1°C */

    /* Parse 12-bit signed internal temperature */
    int16_t raw_int = ((int16_t)(rx[2] & 0xFF) << 4) | (rx[3] >> 4);
    if (raw_int & 0x0800) {
        raw_int |= 0xF000;
    }
    if (internal_temp)
        *internal_temp = raw_int * 625 / 10000;  /* 0.0625°C → ×0.1°C */

    if (fault)
        *fault = rx[3] & 0x07;  /* Fault bits */

    return tc_deci;
}

/* === Moving Average Filter === */
#define AVG_TAPS GS_TC_MOVING_AVG_TAPS
static int16_t temp_buf[4][AVG_TAPS];
static int buf_idx[4] = {0};

static int16_t moving_average(int probe_idx, int16_t new_val)
{
    temp_buf[probe_idx][buf_idx[probe_idx]] = new_val;
    buf_idx[probe_idx] = (buf_idx[probe_idx] + 1) % AVG_TAPS;

    int32_t sum = 0;
    for (int i = 0; i < AVG_TAPS; i++)
        sum += temp_buf[probe_idx][i];
    return (int16_t)(sum / AVG_TAPS);
}

/* === Probe State === */
static uint8_t g_probe_id = 0xFF;
static uint8_t g_meat_type = GS_MEAT_BEEF;
static uint8_t g_doneness_level = GS_DONENESS_MEDIUM;
static int16_t g_target_temp = 600;  /* 60.0°C default */
static uint8_t g_battery_mv = 0;
static uint8_t g_joined = 0;

/* Temperature readings */
static int16_t g_temp_tip = 0;
static int16_t g_temp_mid = 0;
static int16_t g_temp_surface = 0;
static int16_t g_temp_ambient = 0;

/* Doneness prediction state */
static uint8_t g_predicted_doneness = GS_DONENESS_RAW;
static uint16_t g_doneness_eta_10s = 0;
static uint16_t g_msg_seq = 0;

/* === Battery Voltage === */
static uint16_t read_battery_mv(void)
{
    /* ADC read of VBAT pin */
    /* In production: adc_read(adc_dev, &seq) */
    return 4200;  /* Simulated 4.2V */
}

/* === Doneness Prediction (simplified) === */
/*
 * DonenessNet: 1D-CNN on 4-channel thermal history (90s × 2Hz = 180 timesteps).
 * On nRF52840: use TFLite-Micro or simplified heuristic.
 *
 * Simplified heuristic: predict based on current tip temp and rate of change.
 */
static void predict_doneness(void)
{
    /* Rate of temperature change (°C per 10s) */
    static int16_t prev_tip = 0;
    int16_t rate = (g_temp_tip - prev_tip) / 10;  /* ×0.1°C per 10s */
    prev_tip = g_temp_tip;

    /* Determine current doneness level from temp */
    if (g_meat_type < GS_MEAT_TYPE_COUNT) {
        for (int d = GS_DONENESS_WELL; d >= GS_DONENESS_RARE; d--) {
            int16_t target = gs_doneness_temp[g_meat_type][d];
            if (target > 0 && g_temp_tip >= target) {
                g_predicted_doneness = d;
                break;
            }
        }
    }

    /* ETA: time to reach target in ×10s units */
    if (g_target_temp > 0 && g_temp_tip < g_target_temp && rate > 0) {
        g_doneness_eta_10s = (g_target_temp - g_temp_tip) / rate;
        if (g_doneness_eta_10s > 9999)
            g_doneness_eta_10s = 9999;
    } else {
        g_doneness_eta_10s = 0;
    }

    /* Probe cable overtemp check */
    if (g_temp_surface > GS_TC_OVERTEMP_C * 10) {
        LOG_WRN("Probe cable overtemp: %.1f°C", g_temp_surface / 10.0);
        /* Power off probe thermocouples */
        /* gpio_pin_set_dt(&probe_en, 0); */
    }
}

/* === BLE GATT: Send telemetry to Hub === */
/*
 * BLE GATT Service: GrillSync Probe
 * - Characteristic: Telemetry (notify, 18 bytes)
 * - Characteristic: Config (write, 4 bytes: probe_id, meat_type, doneness, target)
 */
static void ble_send_telemetry(void)
{
    /* Encode telemetry message */
    gs_message_t msg;
    gs_build_probe_telem(&msg, g_probe_id, g_msg_seq++,
                           g_battery_mv / 10, g_probe_id, g_meat_type,
                           g_temp_tip, g_temp_mid, g_temp_surface,
                           g_temp_ambient, g_target_temp,
                           g_predicted_doneness, g_doneness_eta_10s, 0xFF);

    /* In production: bt_gatt_notify(conn, attrs, &msg.payload, msg.payload_len) */
    LOG_INF("BLE TX: probe=%d tip=%.1f°C mid=%.1f°C done=%d eta=%ds",
            g_probe_id, g_temp_tip / 10.0, g_temp_mid / 10.0,
            g_predicted_doneness, g_doneness_eta_10s * 10);
}

/* === BLE Connection Callbacks === */
static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        LOG_ERR("BLE connection failed (err %u)", err);
        return;
    }
    g_joined = 1;
    LOG_INF("BLE connected to Hub");
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    g_joined = 0;
    LOG_INF("BLE disconnected (reason %u)", reason);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

/* === Sensor Reading Task === */
static void sensor_task(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1); ARG_UNUSED(arg2); ARG_UNUSED(arg3);

    int16_t internal;
    uint8_t fault;

    while (1) {
        /* Read 4 thermocouples */
        int16_t tip_raw = read_max31855(0, &internal, &fault);
        int16_t mid_raw = read_max31855(1, &internal, &fault);
        int16_t surf_raw = read_max31855(2, &internal, &fault);
        int16_t amb_raw = read_max31855(3, &internal, &fault);

        /* Check for faults */
        if (fault & 0x01) {
            LOG_WRN("TC open circuit on probe %d", 0);
            /* Send alert via BLE */
        }
        if (fault & 0x02) {
            LOG_WRN("TC short to GND on probe %d", 0);
        }

        /* Apply moving average filter */
        g_temp_tip = moving_average(0, tip_raw);
        g_temp_mid = moving_average(1, mid_raw);
        g_temp_surface = moving_average(2, surf_raw);
        g_temp_ambient = moving_average(3, amb_raw);

        /* Read battery */
        g_battery_mv = read_battery_mv();
        if (g_battery_mv < GS_PROBE_LOW_BATTERY_MV / 10) {
            LOG_WRN("Low battery: %dmV", g_battery_mv * 10);
        }

        /* Predict doneness */
        predict_doneness();

        /* Send telemetry via BLE */
        if (g_joined) {
            ble_send_telemetry();
        }

        LOG_INF("Temps: tip=%.1f°C mid=%.1f°C surf=%.1f°C amb=%.1f°C done=%d eta=%ds",
                g_temp_tip / 10.0, g_temp_mid / 10.0,
                g_temp_surface / 10.0, g_temp_ambient / 10.0,
                g_predicted_doneness, g_doneness_eta_10s * 10);

        /* 2 Hz during active cook */
        k_sleep(K_MSEC(500));
    }
}

/* === Main === */
int main(void)
{
    LOG_INF("GrillSync Meat Probe starting...");

    /* Initialize SPI */
    spi_dev = DEVICE_DT_GET(SPI_DEV_ID);
    if (!device_is_ready(spi_dev)) {
        LOG_ERR("SPI device not ready");
        return -1;
    }

    /* Initialize GPIO for CS pins */
    gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
    for (int i = 0; i < 4; i++) {
        if (!gpio_is_ready_dt(&tc_cs[i])) {
            LOG_ERR("CS GPIO %d not ready", i);
            return -1;
        }
        gpio_pin_configure_dt(&tc_cs[i], GPIO_OUTPUT_INACTIVE);
    }

    /* Initialize temp buffers */
    memset(temp_buf, 0, sizeof(temp_buf));

    /* Initialize BLE */
    int err = bt_enable(NULL);
    if (err) {
        LOG_ERR("BLE init failed (err %d)", err);
        return -1;
    }

    /* Start advertising */
    /* In production: bt_le_adv_start with GrillSync service UUID */
    LOG_INF("BLE advertising started");

    /* Start sensor task */
    k_thread_create(&sensor_thread, sensor_stack,
                    K_THREAD_STACK_SIZEOF(sensor_stack),
                    sensor_task, NULL, NULL, NULL,
                    K_PRIO_PREEMPT(5), 0, K_NO_WAIT);

    LOG_INF("Meat Probe ready. Waiting for Hub connection...");

    while (1) {
        k_sleep(K_SECONDS(1));
    }

    return 0;
}

/* Thread definitions */
#define SENSOR_STACK_SIZE 2048
K_THREAD_STACK_DEFINE(sensor_stack, SENSOR_STACK_SIZE);
static struct k_thread sensor_thread;