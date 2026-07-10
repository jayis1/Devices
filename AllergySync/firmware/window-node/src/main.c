/*
 * AllergySync — Window Node Firmware (nRF52840, Zephyr RTOS)
 *
 * Actuator node at each operable window:
 *  - Stepper motor control (TMC2209) for window actuator
 *  - Reed switch for open/closed state feedback
 *  - Relay output for air purifier trigger
 *  - VEML7700 ambient light sensor
 *  - INA260 current/voltage monitor (battery)
 *  - Low-power sleep between Sub-GHz wake events
 *  - 4× AA NiMH or USB power
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/random/rand32.h>

#include "common/allergysync_proto.h"
#include "common/as_lr1121.h"
#include "common/as_tdma.h"

LOG_MODULE_REGISTER(window_node, LOG_LEVEL_INF);

/* ---- Pin definitions (nRF52840, custom board) ---- */
#define PIN_LR_CS       4
#define PIN_LR_SCLK     5
#define PIN_LR_MISO     6
#define PIN_LR_MOSI     7
#define PIN_LR_DIO0     8
#define PIN_LR_DIO1     9
#define PIN_LR_BUSY     10
#define PIN_LR_RESET    11

#define PIN_STEP        13
#define PIN_DIR         14
#define PIN_STEP_EN     15
#define PIN_REED        19
#define PIN_RELAY       20
#define PIN_BUTTON      27

#define PIN_VEML_SDA    21
#define PIN_VEML_SCL    22
#define PIN_INA_SDA     24
#define PIN_INA_SCL     25
#define PIN_LED         26

/* ---- Device labels (Zephyr devicetree) ---- */
#define SPI_DEV     DT_NODELABEL(spi0)
#define I2C_DEV     DT_NODELABEL(i2c0)
#define GPIO_PORT  DT_NODELABEL(gpio0)

static const struct device *spi_dev;
static const struct device *i2c_dev;
static const struct device *gpio_dev;

/* ---- Stepper state ---- */
typedef struct {
    uint8_t  position_pct;  /* 0=closed, 100=fully open */
    bool     moving;
    bool     calibrated;
    int16_t  step_count;    /* Current step count from closed */
    uint16_t total_steps;   /* Total steps for full open */
} stepper_t;

static stepper_t stepper = {
    .position_pct = 0,
    .moving = false,
    .calibrated = false,
    .total_steps = 2000, /* 200 steps/rev × 10 revs = 2000 for full open */
};

/* ---- Node state ---- */
static as_tdma_node_t tdma;
static uint8_t relay_state = 0;
static uint16_t light_lux = 0;
static uint16_t battery_mv = 0;
static uint8_t  battery_pct = 100;

/* ---- LR1121 platform port ---- */
static void lr_cs_select(void)
{
    gpio_pin_set(gpio_dev, PIN_LR_CS, 0);
}
static void lr_cs_release(void)
{
    gpio_pin_set(gpio_dev, PIN_LR_CS, 1);
}
static void lr_spi_xfer(const uint8_t *tx, uint8_t *rx, size_t len)
{
    const struct spi_config cfg = {
        .frequency = 8000000,
        .operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB,
        .cs = NULL, /* We manage CS manually */
    };
    struct spi_buf tx_buf = { .buf = (void *)tx, .len = len };
    struct spi_buf_set tx_set = { .buffers = &tx_buf, .count = 1 };
    struct spi_buf rx_buf = { .buf = rx, .len = len };
    struct spi_buf_set rx_set = { .buffers = &rx_buf, .count = 1 };

    gpio_pin_set(gpio_dev, PIN_LR_CS, 0);
    spi_transceive(spi_dev, &cfg, &tx_set, &rx_set);
    gpio_pin_set(gpio_dev, PIN_LR_CS, 1);
}
static void lr_reset(bool assert)
{
    gpio_pin_set(gpio_dev, PIN_LR_RESET, assert ? 0 : 1);
}
static bool lr_busy_read(void)
{
    return gpio_pin_get(gpio_dev, PIN_LR_BUSY) != 0;
}
static void lr_delay_ms(uint32_t ms)
{
    k_msleep(ms);
}

static as_lr1121_port_t lr_port = {
    .cs_select  = lr_cs_select,
    .cs_release = lr_cs_release,
    .spi_xfer   = lr_spi_xfer,
    .reset      = lr_reset,
    .busy_read  = lr_busy_read,
    .delay_ms   = lr_delay_ms,
};

/* ---- Stepper motor ---- */
static void stepper_step(int16_t steps)
{
    /* Direction */
    gpio_pin_set(gpio_dev, PIN_DIR, steps >= 0 ? 1 : 0);

    /* Enable driver */
    gpio_pin_set(gpio_dev, PIN_STEP_EN, 0); /* Active low */

    uint16_t abs_steps = steps < 0 ? -steps : steps;
    for (uint16_t i = 0; i < abs_steps; i++) {
        gpio_pin_set(gpio_dev, PIN_STEP, 1);
        k_busy_wait(800); /* 800 µs pulse = ~1250 steps/s */
        gpio_pin_set(gpio_dev, PIN_STEP, 0);
        k_busy_wait(800);
    }

    gpio_pin_set(gpio_dev, PIN_STEP_EN, 1); /* Disable */
    stepper.step_count += steps;
    stepper.position_pct = (uint8_t)(
        (100 * stepper.step_count) / (int)stepper.total_steps);
    if (stepper.position_pct > 100) stepper.position_pct = 100;
}

static void stepper_close(void)
{
    if (stepper.position_pct == 0)
        return;
    stepper_step(-stepper.step_count); /* Move to 0 */
    LOG_INF("Window closed");
}

static void stepper_open(uint8_t pct)
{
    if (pct > 100) pct = 100;
    int16_t target = (stepper.total_steps * pct) / 100;
    int16_t delta = target - stepper.step_count;
    stepper_step(delta);
    LOG_INF("Window opened to %d%%", pct);
}

/* ---- VEML7700 (ambient light, I2C) ---- */
static uint16_t veml7700_read_lux(void)
{
    /* VEML7700: config reg 0x00 → power on, ALS integration 100ms */
    /* Read ALS data reg 0x04 */
    uint8_t reg = 0x04;
    uint8_t data[2];
    i2c_write_read(i2c_dev, 0x10, &reg, 1, data, 2);
    uint16_t raw = (data[1] << 8) | data[0];
    /* Convert: lux = raw × 0.0036 (for 100ms integration, gain 1/8) */
    return (uint16_t)(raw * 0.0036 * 100); /* × 100 for resolution */
}

/* ---- INA260 (current/voltage, I2C) ---- */
static void ina260_read(uint16_t *vbus_mv, uint16_t *current_ma)
{
    uint8_t reg = 0x01; /* Bus voltage register */
    uint8_t data[2];
    i2c_write_read(i2c_dev, 0x40, &reg, 1, data, 2);
    *vbus_mv = ((data[0] << 8) | data[1]) * 1; /* 1.25 mV/LSB */

    reg = 0x02; /* Current register */
    i2c_write_read(i2c_dev, 0x40, &reg, 1, data, 2);
    *current_ma = ((data[0] << 8) | data[1]) * 5; /* 1.25 mA/LSB × 4 */
}

static uint8_t compute_battery_pct(uint16_t mv)
{
    /* 4× AA NiMH: 4.8V nominal, 4.0V empty, 5.6V full */
    if (mv >= 5600) return 100;
    if (mv <= 4000) return 0;
    return (uint8_t)(100 * (mv - 4000) / 1600);
}

/* ---- Reed switch (window state) ---- */
static uint8_t read_reed(void)
{
    return gpio_pin_get(gpio_dev, PIN_REED) ? 1 : 0;
    /* 1 = closed (magnet near reed), 0 = open */
}

/* ---- Calibration routine ---- */
static void calibrate(void)
{
    LOG_INF("Calibration: closing window...");
    /* Drive motor until reed switch triggers (closed position) */
    stepper_close();
    k_msleep(500);
    if (read_reed()) {
        stepper.calibrated = true;
        stepper.position_pct = 0;
        stepper.step_count = 0;
        LOG_INF("Calibration done: window closed, reed confirmed");
    } else {
        LOG_WRN("Calibration: reed switch not triggered");
    }
}

/* ---- Command handler ---- */
static void handle_command(const as_command_t *cmd)
{
    switch (cmd->cmd_type) {
    case AS_CMD_CLOSE_WINDOW:
        stepper_close();
        break;
    case AS_CMD_OPEN_WINDOW:
        stepper_open(100);
        break;
    case AS_CMD_SET_POSITION:
        stepper_open(cmd->param);
        break;
    case AS_CMD_PURIFIER_ON:
        gpio_pin_set(gpio_dev, PIN_RELAY, 1);
        relay_state = 1;
        LOG_INF("Purifier ON");
        break;
    case AS_CMD_PURIFIER_OFF:
        gpio_pin_set(gpio_dev, PIN_RELAY, 0);
        relay_state = 0;
        LOG_INF("Purifier OFF");
        break;
    case AS_CMD_RECALIBRATE:
        calibrate();
        break;
    case AS_CMD_REBOOT:
        LOG_INF("Reboot requested");
        NVIC_SystemReset();
        break;
    default:
        LOG_WRN("Unknown command: 0x%02X", cmd->cmd_type);
        break;
    }
}

/* ---- TDMA RX callback ---- */
static void on_mesh_rx(const as_header_t *hdr, const uint8_t *payload,
                       size_t len, int8_t rssi)
{
    LOG_INF("RX: type=0x%02X from=%d rssi=%d len=%d",
            hdr->msg_type, hdr->src_id, rssi, (int)len);

    if (hdr->msg_type == AS_MSG_COMMAND) {
        const as_command_t *cmd = (const as_command_t *)payload;
        handle_command(cmd);

        /* Send ACK */
        uint8_t ack = 0x01;
        as_tdma_send(&tdma, AS_MSG_ACK, hdr->src_id, &ack, 1);
    }
}

/* ---- Telemetry task ---- */
static void telemetry_work(struct k_work *work)
{
    /* Read sensors */
    light_lux = veml7700_read_lux();
    ina260_read(&battery_mv, NULL);
    battery_pct = compute_battery_pct(battery_mv);
    uint8_t reed = read_reed();

    /* Build telemetry */
    as_telem_window_t telem;
    memset(&telem, 0, sizeof(telem));
    telem.window_state = reed ? 0 : (stepper.position_pct > 0 ? 1 : 0);
    telem.position_pct = stepper.position_pct;
    telem.light_lux   = light_lux;
    telem.battery_mv  = battery_mv;
    telem.battery_pct = battery_pct;
    telem.relay_state = relay_state;
    telem.motor_fault  = 0;

    /* Send to hub */
    as_tdma_send(&tdma, AS_MSG_TELEMETRY, 0,
                 (uint8_t *)&telem, sizeof(telem));
    LOG_INF("Telemetry: state=%d pos=%d%% light=%u batt=%dmV relay=%d",
            telem.window_state, telem.position_pct, light_lux,
            battery_mv, relay_state);
}

K_WORK_DELAYABLE_DEFINE(telemetry_work, telemetry_work);

/* ---- Main ---- */
int main(void)
{
    LOG_INF("AllergySync Window Node starting (nRF52840)...");

    /* Get device bindings */
    spi_dev = DEVICE_DT_GET(SPI_DEV);
    i2c_dev = DEVICE_DT_GET(I2C_DEV);
    gpio_dev = DEVICE_DT_GET(GPIO_PORT);

    if (!device_is_ready(spi_dev) || !device_is_ready(i2c_dev) ||
        !device_is_ready(gpio_dev)) {
        LOG_ERR("Device not ready");
        return -1;
    }

    /* Configure GPIO */
    gpio_pin_configure(gpio_dev, PIN_LR_CS, GPIO_OUTPUT_HIGH);
    gpio_pin_configure(gpio_dev, PIN_LR_RESET, GPIO_OUTPUT_HIGH);
    gpio_pin_configure(gpio_dev, PIN_LR_BUSY, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_configure(gpio_dev, PIN_LR_DIO0, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_configure(gpio_dev, PIN_LR_DIO1, GPIO_INPUT | GPIO_PULL_UP);

    gpio_pin_configure(gpio_dev, PIN_STEP, GPIO_OUTPUT_LOW);
    gpio_pin_configure(gpio_dev, PIN_DIR, GPIO_OUTPUT_LOW);
    gpio_pin_configure(gpio_dev, PIN_STEP_EN, GPIO_OUTPUT_HIGH);
    gpio_pin_configure(gpio_dev, PIN_RELAY, GPIO_OUTPUT_LOW);
    gpio_pin_configure(gpio_dev, PIN_LED, GPIO_OUTPUT_LOW);

    gpio_pin_configure(gpio_dev, PIN_REED, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_configure(gpio_dev, PIN_BUTTON, GPIO_INPUT | GPIO_PULL_UP);

    /* Init LR1121 */
    if (as_lr1121_init(&lr_port) != 0) {
        LOG_ERR("LR1121 init failed!");
        return -1;
    }
    as_lr1121_set_channel(868100000);
    as_lr1121_set_tx_power(14);
    as_lr1121_set_modem_fsk(50000, 25000, 100000);
    uint8_t sync[] = { 0xA5, 0x1E, 0x9C, 0x47 };
    as_lr1121_set_sync_word(sync, 4);

    /* Init TDMA */
    as_tdma_init(&tdma, false, &lr_port);

    /* Join mesh */
    uint8_t pubkey[64] = {0};
    as_tdma_join(&tdma, AS_NODE_WINDOW, pubkey);

    /* Auto-calibrate on boot */
    calibrate();

    /* Schedule telemetry every 10 minutes */
    k_work_schedule(&telemetry_work, K_MINUTES(10));

    LOG_INF("Window Node running. Slot=%d, ID=%d", tdma.slot, tdma.node_id);

    /* Main loop: sleep and wake on Sub-GHz IRQ */
    while (1) {
        /* Enter RX to catch hub commands */
        uint8_t rx_buf[256];
        as_radio_pkt_info_t info;
        int ret = as_lr1121_rx(rx_buf, &info, AS_MESH_FRAME_MS);
        if (ret == AS_RADIO_OK) {
            as_tdma_handle_rx(&tdma, rx_buf, info.length,
                              info.rssi, on_mesh_rx);
        }

        /* Low-power sleep between frames */
        as_lr1121_sleep();
        k_sleep(K_SECONDS(6));
        as_lr1121_standby();
    }

    return 0;
}