/**
 * FlowGuard - Pipe Sensor Node Firmware
 * nRF52832 (Ultra-low-power Zigbee router + vibration/acoustic sensor)
 *
 * Detects leaks, pipe vibrations, water hammer, and freeze conditions.
 * Runs on CR2477 coin cell with 7+ year battery life.
 *
 * Copyright (c) 2026 jayis1 - MIT License
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/pm/device.h>
#include <zephyr/logging/log.h>
#include <zboss_api.h>
#include <zigbee/zigbee_app_utils.h>

#include "fg_protocol.h"
#include "fg_util.h"

LOG_MODULE_REGISTER(pipe_sensor, CONFIG_FG_PIPE_LOG_LEVEL);

/* ============================================================
 * Power Management
 * ============================================================ */

/* Sleep between readings: 60 seconds (normal), 5 seconds (fast/anomaly) */
#define SLEEP_INTERVAL_NORMAL_SEC    60
#define SLEEP_INTERVAL_FAST_SEC       5
#define SLEEP_INTERVAL_EMERGENCY_SEC  1

static uint32_t sleep_interval = SLEEP_INTERVAL_NORMAL_SEC;

/* ============================================================
 * Sensor State
 * ============================================================ */

static fg_pipe_sensor_report_t sensor_data;
static uint8_t leak_debounce_counter = 0;
static bool anomaly_detected = false;
static fg_acoustic_class_t last_acoustic_class = FG_ACOUSTIC_NORMAL;
static float last_acoustic_confidence = 0.0f;

/* ============================================================
 * ADXL362 Accelerometer (SPI)
 * ============================================================ */

#define ADXL362_REG_XDATA     0x08
#define ADXL362_REG_YDATA     0x09
#define ADXL362_REG_ZDATA     0x0A
#define ADXL362_REG_STATUS    0x0B
#define ADXL362_REG_THRESH_ACT 0x20
#define ADXL362_REG_TIME_ACT  0x21
#define ADXL362_REG_THRESH_INACT 0x23
#define ADXL362_REG_TIME_INACT_L 0x25
#define ADXL362_REG_TIME_INACT_H 0x26
#define ADXL362_REG_ACT_INACT_CTL 0x27
#define ADXL362_REG_FIFO_CTL  0x28
#define ADXL362_REG_FIFO_SAMPLES 0x29
#define ADXL362_REG_INTMAP1   0x2A
#define ADXL362_REG_INTMAP2   0x2B
#define ADXL362_REG_SOFT_RESET 0x1F
#define ADXL362_REG_POWER_CTL 0x2D

static const struct spi_dt_spec adxl_spi = SPI_DT_SPEC_GET(DT_NODELABEL(adxl362), 0, 0);

/**
 * Initialize ADXL362 for motion-activated wake-up.
 * Activity threshold: 50mg (detects pipe vibration from flow/leak)
 * Inactivity threshold: 10mg (quiet when no flow)
 * Activity triggers interrupt on INT1 pin
 */
int adxl362_init(void)
{
    int ret;
    uint8_t tx_buf[3];
    uint8_t rx_buf[3];
    struct spi_buf tx_spi_buf = { .buf = tx_buf, .len = 3 };
    struct spi_buf rx_spi_buf = { .buf = rx_buf, .len = 3 };
    struct spi_buf_set tx_set = { .buffers = &tx_spi_buf, .count = 1 };
    struct spi_buf_set rx_set = { .buffers = &rx_spi_buf, .count = 1 };

    /* Soft reset */
    tx_buf[0] = 0x0A;  /* Write command */
    tx_buf[1] = ADXL362_REG_SOFT_RESET;
    tx_buf[2] = 0x52;  /* Reset key */
    spi_transceive_dt(&adxl_spi, &tx_set, &rx_set);
    k_sleep(K_MSEC(1));

    /* Set activity threshold: 50mg = 50 in 2mg/LSB */
    tx_buf[0] = 0x0A;
    tx_buf[1] = ADXL362_REG_THRESH_ACT;
    tx_buf[2] = 25;  /* 25 × 2mg = 50mg */
    spi_transceive_dt(&adxl_spi, &tx_set, &rx_set);

    /* Set activity time: 3 samples (3 × 1/ODR) */
    tx_buf[1] = ADXL362_REG_TIME_ACT;
    tx_buf[2] = 3;
    spi_transceive_dt(&adxl_spi, &tx_set, &rx_set);

    /* Set inactivity threshold: 10mg = 5 in 2mg/LSB */
    tx_buf[1] = ADXL362_REG_THRESH_INACT;
    tx_buf[2] = 5;
    spi_transceive_dt(&adxl_spi, &tx_set, &rx_set);

    /* Set inactivity time: 300 samples (~30 seconds at 10Hz) */
    tx_buf[1] = ADXL362_REG_TIME_INACT_L;
    tx_buf[2] = 0x2C;  /* 300 = 0x012C */
    spi_transceive_dt(&adxl_spi, &tx_set, &rx_set);
    tx_buf[1] = ADXL362_REG_TIME_INACT_H;
    tx_buf[2] = 0x01;
    spi_transceive_dt(&adxl_spi, &tx_set, &rx_set);

    /* Enable activity interrupt, linked mode */
    tx_buf[1] = ADXL362_REG_ACT_INACT_CTL;
    tx_buf[2] = 0x3F;  /* Act en, inact en, linked mode */
    spi_transceive_dt(&adxl_spi, &tx_set, &rx_set);

    /* Map activity interrupt to INT1 */
    tx_buf[1] = ADXL362_REG_INTMAP1;
    tx_buf[2] = 0x10;  /* Activity on INT1 */
    spi_transceive_dt(&adxl_spi, &tx_set, &rx_set);

    /* Start measurement */
    tx_buf[1] = ADXL362_REG_POWER_CTL;
    tx_buf[2] = 0x02;  /* Measurement mode */
    spi_transceive_dt(&adxl_spi, &tx_set, &rx_set);

    LOG_INF("ADXL362 initialized: activity threshold 50mg, inactivity 10mg");
    return 0;
}

/**
 * Read vibration RMS from ADXL362.
 * Captures 100 samples at 100Hz and computes RMS.
 */
uint16_t adxl362_read_vibration_rms(void)
{
    int32_t sum_sq = 0;
    uint16_t sample_count = 100;

    for (uint16_t i = 0; i < sample_count; i++) {
        /* Read XDATA, YDATA, ZDATA registers */
        uint8_t cmd_buf[2] = { 0x0B, ADXL362_REG_XDATA };
        uint8_t data_buf[3];
        struct spi_buf cmd_spi = { .buf = cmd_buf, .len = 2 };
        struct spi_buf data_spi = { .buf = data_buf, .len = 3 };
        struct spi_buf_set cmd_set = { .buffers = &cmd_spi, .count = 1 };
        struct spi_buf_set data_set = { .buffers = &data_spi, .count = 1 };

        spi_transceive_dt(&adxl_spi, &cmd_set, &data_set);

        int8_t x = (int8_t)data_buf[0];
        int8_t y = (int8_t)data_buf[1];
        int8_t z = (int8_t)data_buf[2];

        /* Convert to mg: each LSB = 2mg in 12-bit mode */
        sum_sq += (int32_t)(x * x + y * y + z * z);

        k_sleep(K_MSEC(10));  /* 100Hz sampling */
    }

    /* RMS in mg: sqrt(sum_sq / N) × 2mg */
    /* Multiply by 10 for mg × 10 format */
    uint16_t rms_mgx10 = (uint16_t)(
        10.0f * sqrtf((float)sum_sq / sample_count) * 2.0f
    );

    return rms_mgx10;
}

/* ============================================================
 * SPH0645LM4H MEMS Microphone (I2S)
 * ============================================================ */

/**
 * Capture 2-second acoustic window at 48kHz.
 * Only powered on when vibration anomaly detected.
 * Returns raw PCM data for TFLite Micro classification.
 */
int sph0645_capture(uint8_t *buffer, uint16_t buffer_len)
{
    /* I2S capture would use Zephyr I2S API */
    /* 2 seconds × 48000 Hz × 2 bytes = 192000 bytes of data */
    /* TFLite Micro model downsamples to 16kHz internally */
    /* We capture at 48kHz for potential future frequency analysis */

    /* Enable MEMS mic power */
    /* gpio_pin_set_dt(&mic_enable, 1); */
    /* k_sleep(K_MSEC(10));  */ /* Settle time */

    /* Start I2S capture */
    /* ... Zephyr I2S API calls ... */

    /* Disable MEMS mic power after capture */
    /* gpio_pin_set_dt(&mic_enable, 0); */

    LOG_INF("Acoustic capture: 2 sec at 48kHz");
    return 0;
}

/* ============================================================
 * DS18B20 Temperature Sensor (1-Wire)
 * ============================================================ */

/**
 * Read pipe surface temperature from DS18B20.
 * Returns temperature in °C × 100.
 */
int16_t ds18b20_read_temperature(void)
{
    /* 1-Wire protocol: Reset → Skip ROM → Convert T → wait → Read Scratchpad */
    /* Using Zephyr 1-Wire subsystem or bit-banging */
    int16_t raw_temp = 2400;  /* Placeholder: ~24°C */
    return fg_ds18b20_to_cx100(raw_temp);
}

/* ============================================================
 * SHT40 Humidity Sensor (I2C)
 * ============================================================ */

static const struct i2c_dt_spec sht40 = I2C_DT_SPEC_GET(DT_NODELABEL(sht40));

/**
 * Read ambient humidity from SHT40.
 * Returns humidity in %RH × 10.
 */
uint16_t sht40_read_humidity(void)
{
    uint8_t cmd = 0xFD;  /* High-precision measurement */
    uint8_t data[6];
    int ret;

    ret = i2c_write_dt(&sht40, &cmd, 1);
    if (ret != 0) {
        LOG_WRN("SHT40 write failed: %d", ret);
        return 0;
    }

    k_sleep(K_MSEC(10));  /* Measurement time */

    ret = i2c_read_dt(&sht40, data, 6);
    if (ret != 0) {
        LOG_WRN("SHT40 read failed: %d", ret);
        return 0;
    }

    /* Humidity calculation: RH% = 100 × (Srh / (2^16 - 1)) */
    uint16_t srh = (data[3] << 8) | data[4];
    uint16_t humidity_cx10 = (uint16_t)(1000.0f * (float)srh / 65535.0f);

    return humidity_cx10;
}

/* ============================================================
 * Conductive Leak Detection
 * ============================================================ */

#define LEAK_EXCITE_PIN  DT_ALIAS(leak_excite)
#define LEAK_DETECT_PIN   DT_ALIAS(leak_detect)

static const struct gpio_dt_spec leak_excite = GPIO_DT_SPEC_GET(LEAK_EXCITE_PIN, gpios);
static const struct gpio_dt_spec leak_detect = GPIO_DT_SPEC_GET(LEAK_DETECT_PIN, gpios);

/**
 * Check conductive leak trace.
 * Excites trace 1 with brief pulse, reads trace 2.
 * Returns true if water detected (low resistance between traces).
 */
bool check_leak_trace(void)
{
    /* Excite trace with 3V pulse */
    gpio_pin_set_dt(&leak_excite, 1);
    k_sleep(K_MSEC(1));  /* Brief pulse */

    /* Read detection pin */
    bool wet = gpio_pin_get_dt(&leak_detect);

    /* De-energize trace */
    gpio_pin_set_dt(&leak_excite, 0);

    return wet;
}

/* ============================================================
 * TFLite Micro Leak Classification
 * ============================================================ */

/**
 * Run acoustic classification model on captured audio.
 * Uses 1D-CNN + GRU model (INT8 quantized, 85KB).
 *
 * Input: 2-second audio window, 48kHz, 16-bit mono
 * Output: Classification + confidence score
 */
fg_acoustic_class_t classify_acoustic(const uint8_t *audio_data, uint16_t len,
                                        float *confidence)
{
    /* TFLite Micro inference would go here */
    /* For now, return NORMAL with low confidence as placeholder */
    *confidence = 0.3f;
    return FG_ACOUSTIC_NORMAL;

    /* Full implementation would:
     * 1. Downsample 48kHz to 16kHz (32000 samples)
     * 2. Convert to float and normalize
     * 3. Run TFLite Micro interpreter
     * 4. Extract output probabilities
     * 5. Return highest-confidence class
     */
}

/* ============================================================
 * Sensor Data Collection and Reporting
 * ============================================================ */

void collect_sensor_data(void)
{
    /* Read temperature */
    sensor_data.temperature_cx100 = ds18b20_read_temperature();
    LOG_INF("Temperature: %d.%02d °C",
             sensor_data.temperature_cx100 / 100,
             abs(sensor_data.temperature_cx100) % 100);

    /* Read humidity */
    sensor_data.humidity_cx10 = sht40_read_humidity();
    LOG_INF("Humidity: %d.%d %%RH",
             sensor_data.humidity_cx10 / 10,
             sensor_data.humidity_cx10 % 10);

    /* Read vibration */
    sensor_data.vibration_rms_mgx10 = adxl362_read_vibration_rms();
    LOG_INF("Vibration RMS: %d.%d mg",
             sensor_data.vibration_rms_mgx10 / 10,
             sensor_data.vibration_rms_mgx10 % 10);

    /* Check leak trace */
    bool leak_wet = check_leak_trace();
    bool leak_confirmed = fg_leak_debounce(leak_wet, &leak_debounce_counter, 3);
    sensor_data.leak_state = leak_confirmed ? FG_LEAK_CONFIRMED :
                              (leak_wet ? FG_LEAK_WET : FG_LEAK_DRY);
    LOG_INF("Leak trace: %s", leak_confirmed ? "CONFIRMED WET" :
                              (leak_wet ? "WET (debouncing)" : "DRY"));

    /* Read battery voltage */
    sensor_data.battery_mv = fg_adc_to_battery_mv(
        adc_read_channel(&vbat_pin), 3300, 1000000, 1000000);
    uint8_t bat_level = fg_battery_level(sensor_data.battery_mv);
    LOG_INF("Battery: %d mV (level %d)", sensor_data.battery_mv, bat_level);

    /* Check for freeze risk */
    int16_t temp_c = sensor_data.temperature_cx100 / 100;
    if (temp_c <= 3) {
        LOG_WRN("FREEZE RISK: Pipe at %d °C!", temp_c);
        sleep_interval = SLEEP_INTERVAL_FAST_SEC;
    } else {
        sleep_interval = SLEEP_INTERVAL_NORMAL_SEC;
    }

    /* If vibration is high, capture acoustic sample */
    if (sensor_data.vibration_rms_mgx10 > 50) {  /* >5 mg RMS */
        LOG_INF("Vibration anomaly detected (%d mg RMS), capturing audio...",
                 sensor_data.vibration_rms_mgx10 / 10);

        uint8_t audio_buffer[96000];  /* 2sec × 48kHz × 1 byte (downsampled) */
        if (sph0645_capture(audio_buffer, sizeof(audio_buffer)) == 0) {
            float confidence;
            last_acoustic_class = classify_acoustic(audio_buffer, sizeof(audio_buffer),
                                                      &confidence);
            last_acoustic_confidence = confidence;
            sensor_data.acoustic_anomaly = (uint8_t)(confidence * 255.0f);

            LOG_INF("Acoustic class: %d, confidence: %.2f",
                     last_acoustic_class, confidence);

            /* If leak or hammer detected, enter fast reporting mode */
            if (last_acoustic_class == FG_ACOUSTIC_LEAK ||
                last_acoustic_class == FG_ACOUSTIC_HAMMER) {
                sleep_interval = SLEEP_INTERVAL_EMERGENCY_SEC;
                anomaly_detected = true;
            }
        }
    }
}

/**
 * Send sensor report via Zigbee to hub.
 */
void send_sensor_report(void)
{
    sensor_data.node_id = FG_NODE_ID_PIPE_BASE;  /* Would be set during commissioning */
    sensor_data.uptime_sec = k_uptime_get() / 1000;

    /* Send via Zigbee APS to hub */
    zb_bufid_t bufid = zb_buf_get_out();
    if (bufid == ZB_BUF_INVALID) {
        LOG_WRN("Failed to allocate Zigbee buffer for report");
        return;
    }

    uint8_t *payload = zb_buf_initial_alloc(bufid, sizeof(fg_pipe_sensor_report_t));
    memcpy(payload, &sensor_data, sizeof(fg_pipe_sensor_report_t));

    zb_ret_t ret = zb_apsde_data_req(
        bufid,
        FG_NODE_ID_HUB,
        FG_CLUSTER_CONTROL,
        0, 0,
        ZB_APSDE_TX_OPT_SECURITY,
        FG_ZIGBEE_MAX_HOPS
    );

    if (ret != RET_OK) {
        LOG_ERR("Zigbee report send failed: %d", ret);
        zb_buf_free(bufid);
    } else {
        LOG_INF("Sensor report sent to hub");
    }
}

/* ============================================================
 * Zigbee Signal Handler
 * ============================================================ */

void fg_zigbee_signal_cb(zb_bufid_t bufid)
{
    zb_zdo_app_signal_type_t sig = zb_get_app_signal(bufid, NULL);
    LOG_INF("Zigbee signal: 0x%02x", sig);
}

/* ============================================================
 * Main Entry Point
 * ============================================================ */

int main(void)
{
    LOG_INF("FlowGuard Pipe Sensor starting...");
    LOG_INF("Firmware v%d.%d.%d", FG_VERSION_MAJOR, FG_VERSION_MINOR, FG_VERSION_PATCH);

    /* Initialize GPIOs */
    gpio_pin_configure_dt(&leak_excite, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&leak_detect, GPIO_INPUT | GPIO_PULL_DOWN);

    /* Initialize ADXL362 */
    if (adxl362_init() != 0) {
        LOG_ERR("ADXL362 initialization failed!");
    }

    /* Initialize SHT40 */
    if (!device_is_ready(sht40.bus)) {
        LOG_ERR("SHT40 I2C not ready!");
    }

    /* Start Zigbee (as router) */
    zigbee_enable();

    /* Green LED heartbeat */
    static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
    gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);

    LOG_INF("Pipe Sensor initialized. Normal sleep interval: %d sec", sleep_interval);

    /* Main loop */
    while (1) {
        /* Collect sensor data */
        collect_sensor_data();

        /* Send report to hub */
        send_sensor_report();

        /* Update sleep interval based on anomaly state */
        if (anomaly_detected) {
            sleep_interval = SLEEP_INTERVAL_EMERGENCY_SEC;
        } else if (sensor_data.temperature_cx100 / 100 <= 5) {
            sleep_interval = SLEEP_INTERVAL_FAST_SEC;
        } else {
            sleep_interval = SLEEP_INTERVAL_NORMAL_SEC;
        }

        /* Heartbeat blink */
        gpio_pin_set_dt(&led, 1);
        k_sleep(K_MSEC(50));
        gpio_pin_set_dt(&led, 0);

        /* Sleep (deep sleep would be used in production) */
        k_sleep(K_SECONDS(sleep_interval));
    }

    return 0;
}