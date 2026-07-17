/*
 * StormSync — Sump Pit Sentinel Firmware
 * ESP32-WROOM-32E, FreeRTOS
 *
 * Monitors sump pit water level (ultrasonic), pump current (CT clamp),
 * pump vibration (ADXL355), flow rate, and water temperature.
 * Sends telemetry every 30s (15s in storm mode) via Sub-GHz mesh.
 * Battery backup (12V SLA) maintains monitoring during power outage.
 *
 * Build: idf.py build with ESP-IDF v5.x
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/adc.h"
#include "driver/pulse_cnt.h"

#include "../common/protocol.h"
#include "../common/sx1262.h"
#include "../common/mesh.h"
#include "../common/config.h"

static const char *TAG = "StormSync-Sump";

/* === Global state === */
static ss_mesh_ctx_t g_mesh;
static uint16_t g_msg_seq = 0;
static uint8_t g_storm_mode = 0;
static uint16_t g_pump_runtime_today = 0; /* minutes */
static uint8_t g_pump_was_running = 0;
static int64_t g_pump_start_time = 0;

/* === SX1262 SPI Interface === */
static spi_device_handle_t g_spi_dev;

static void spi_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = SUMP_GPIO_SX_MOSI,
        .miso_io_num = SUMP_GPIO_SX_MISO,
        .sclk_io_num = SUMP_GPIO_SX_SCK,
        .quadwp_io_num = -1, .quadhd_io_num = -1,
        .max_transfer_sz = 256,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 8000000,
        .mode = 0,
        .spics_io_num = SUMP_GPIO_SX_NSS,
        .queue_size = 4,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &g_spi_dev);
}

static uint8_t spi_transfer(uint8_t byte) {
    uint8_t rx;
    spi_transaction_t t = { .tx_buffer = &byte, .rx_buffer = &rx, .length = 8 };
    spi_device_polling_transmit(g_spi_dev, &t);
    return rx;
}
static void spi_reset(uint8_t assert) {
    gpio_set_level(SUMP_GPIO_SX_RST, assert ? 0 : 1);
}
static void spi_delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
static int spi_dio1_read(void) { return gpio_get_level(SUMP_GPIO_SX_DIO1); }

static const ss_spi_interface_t g_spi_iface = {
    .init = spi_init,
    .cs_select = NULL, .cs_release = NULL,
    .transfer = spi_transfer,
    .reset = spi_reset,
    .delay_ms = spi_delay_ms,
    .dio1_read = spi_dio1_read,
    .dio1_irq_enable = NULL,
};

/* === Ultrasonic Water Level (JSN-SR04T) === */
static uint16_t read_water_level_mm(void)
{
    /* Trigger pulse: 10 µs high */
    gpio_set_level(SUMP_GPIO_US_TRIG, 1);
    ets_delay_us(10);
    gpio_set_level(SUMP_GPIO_US_TRIG, 0);

    /* Wait for echo (simplified — production uses MCPWM capture + timeout) */
    /* Echo pulse width (µs) / 5.8 = distance in mm */
    /* The sensor is mounted at top of pit, pointing down.
     * water_level = pit_depth - distance_to_water
     */
    /* Simplified: return a simulated reading */
    /* In production: use mcpwm_capture_signal_timers */
    uint32_t echo_us = 20000; /* ~345 cm distance → empty pit */
    uint16_t distance_mm = (uint16_t)(echo_us / 5.8);
    uint16_t level_mm = (SUMP_PIT_DEPTH_CM * 10) - distance_mm;
    if (level_mm > (SUMP_PIT_DEPTH_CM * 10)) level_mm = 0;
    return level_mm;
}

/* === Pump Current (SCT-013-030 CT Clamp) === */
static uint16_t read_pump_current_ma(void)
{
    /* SCT-013-030 outputs 1V at 30A → on 3.3V ADC with divider
     * ADC raw → voltage → current
     * Sample at 1 kHz for 20ms (1 AC cycle at 50Hz) → compute RMS
     */
    /* Simplified: return simulated reading */
    /* In production:
     * 1. Sample ADC at 1 kHz for 20ms (20 samples)
     * 2. Compute RMS: sqrt(sum(x^2)/N)
     * 3. Subtract DC offset
     * 4. Scale: V_rms * calibration_factor → current
     */
    uint32_t sum_sq = 0;
    for (int i = 0; i < 20; i++) {
        int raw = adc1_get_raw(ADC1_CHANNEL_0); /* GPIO27 */
        int centered = raw - 2048; /* Remove DC bias */
        sum_sq += (uint32_t)(centered * centered);
        ets_delay_us(1000); /* 1ms = 1kHz */
    }
    float rms = sqrtf((float)sum_sq / 20.0);
    /* Calibration: 30A → 1V → ~1240 ADC counts RMS
     * current_ma = rms * 30000 / 1240
     */
    uint16_t current_ma = (uint16_t)(rms * 24.2);
    return current_ma;
}

/* === Pump Vibration (ADXL355 via SPI) === */
static void read_vibration(uint16_t *rms_mg, uint16_t *peak_mg)
{
    /* ADXL355: 3-axis accelerometer, 20-bit resolution
     * Read 1024 samples at 1 kHz → compute overall RMS + peak
     * In production: SPI burst read of FIFO, store in buffer, compute stats
     */
    /* Simplified: read a few samples and compute */
    float max_val = 0;
    float sum_sq = 0;
    int n = 64; /* Reduced for demo */

    for (int i = 0; i < n; i++) {
        /* Read X-axis data register (0x11, 0x12, 0x13) */
        gpio_set_level(SUMP_GPIO_ADXL_CS, 0);
        spi_transfer(0x11 | 0x80); /* Read X-axis */
        uint8_t h = spi_transfer(0);
        uint8_t m = spi_transfer(0);
        uint8_t l = spi_transfer(0);
        gpio_set_level(SUMP_GPIO_ADXL_CS, 1);

        /* 20-bit signed, 3.9 µg/LSB → convert to mg */
        int32_t raw = ((int32_t)(h << 16) | (m << 8) | l) >> 4;
        if (raw & 0x080000) raw |= 0xF00000; /* Sign extend */
        float mg = raw * 0.0039f; /* µg → mg */

        float abs_val = fabsf(mg);
        if (abs_val > max_val) max_val = abs_val;
        sum_sq += mg * mg;
        ets_delay_us(1000); /* 1 kHz */
    }

    *rms_mg = (uint16_t)(sqrtf(sum_sq / n) * 1000); /* ×0.001 g */
    *peak_mg = (uint16_t)(max_val * 1000);
}

/* === Flow Rate (YF-S201 Hall Effect) === */
static uint16_t read_flow_rate(void)
{
    /* YF-S201: F = pulse_freq / 7.5 (L/min)
     * Count pulses over 1 second window
     */
    /* Simplified: return simulated reading */
    /* In production: use pulse_cnt peripheral or GPIO ISR */
    static uint16_t flow = 0;
    /* If pump is running, simulate some flow */
    return flow;
}

/* === Water Temperature (DS18B20) === */
static int16_t read_water_temp_deci(void)
{
    /* 1-Wire on GPIO32
     * 1. Reset pulse, Skip ROM (0xCC), Convert T (0x44), wait 750ms
     * 2. Reset, Skip ROM, Read Scratchpad (0xBE), read 2 bytes
     */
    /* Simplified: return 15.0°C */
    return 150;
}

/* === Battery Voltage (12V SLA via divider) === */
static uint8_t read_battery_v(void)
{
    int raw = adc1_get_raw(ADC1_CHANNEL_5); /* GPIO33 */
    /* Divider: Vbat × (R2/(R1+R2)) → ADC
     * R1=30k, R2=10k → factor 4
     * V = raw * 3.3/4095 * 4 = raw * 0.00322
     * Return in 0.1V units: V * 10
     */
    float v = raw * 0.0129; /* 3.3/4095 * 4 * 10 */
    return (uint8_t)(v);
}

/* === Mains Power Detection === */
static uint8_t read_mains_ok(void)
{
    return gpio_get_level(SUMP_GPIO_MAINS) ? 1 : 0;
}

/* === Main Measurement & TX Task === */
static void sensor_task(void *arg)
{
    while (1) {
        /* Read all sensors */
        uint8_t  bat_v = read_battery_v();
        uint16_t water_level = read_water_level_mm();
        uint16_t pump_current = read_pump_current_ma();
        uint16_t flow_rate = read_flow_rate();
        int16_t  water_temp = read_water_temp_deci();
        uint16_t vib_rms, vib_peak;
        read_vibration(&vib_rms, &vib_peak);
        uint8_t  mains = read_mains_ok();

        /* Determine pump status */
        uint8_t pump_status = 0;
        if (pump_current > SUMP_PUMP_CURRENT_THRESH) {
            pump_status = 1; /* Running */
            if (!g_pump_was_running) {
                g_pump_start_time = esp_timer_get_time();
                g_pump_was_running = 1;
            }
            if (pump_current > SUMP_PUMP_OVERLOAD_MA) {
                pump_status = 2; /* Fault: overload */
            }
        } else {
            if (g_pump_was_running) {
                /* Pump just stopped — accumulate runtime */
                int64_t runtime_s = (esp_timer_get_time() - g_pump_start_time) / 1000000;
                g_pump_runtime_today += (uint16_t)(runtime_s / 60);
                g_pump_was_running = 0;
            }
        }

        /* LED indicator */
        gpio_set_level(SUMP_GPIO_PUMP_LED, pump_status == 1 ? 1 : 0);

        ESP_LOGI(TAG, "Lvl:%dmA I:%dmA Pump:%d VibRMS:%d Flow:%d T:%.1f Bat:%.1fV",
                 water_level, pump_current, pump_status, vib_rms,
                 flow_rate, water_temp/10.0, bat_v/10.0);

        /* Safety checks */
        uint16_t pit_full_mm = SUMP_PIT_DEPTH_CM * 10;
        uint8_t level_pct = (uint8_t)((water_level * 100) / pit_full_mm);

        if (level_pct >= SUMP_EMERGENCY_PCT) {
            /* EMERGENCY: water near top of pit */
            ss_message_t alert;
            uint8_t data[2] = { (uint8_t)(water_level & 0xFF),
                                (uint8_t)(water_level >> 8) };
            ss_build_alert(&alert, g_mesh.node_id, g_msg_seq++,
                           SS_ALERT_CRITICAL_WATER, 3, data, 2);
            ss_mesh_send(&g_mesh, &alert);
            ESP_LOGE(TAG, "EMERGENCY: Water level %d%%!", level_pct);

            /* Command actuator to close valve + start backup pump */
            ss_message_t cmd;
            ss_build_command(&cmd, g_mesh.node_id, 0xFF, g_msg_seq++,
                             SS_CMD_VALVE_CLOSE, NULL, 0);
            ss_mesh_send(&g_mesh, &cmd);
            ss_build_command(&cmd, g_mesh.node_id, 0xFF, g_msg_seq++,
                             SS_CMD_PUMP_ON, NULL, 0);
            ss_mesh_send(&g_mesh, &cmd);
            ss_build_command(&cmd, g_mesh.node_id, 0xFF, g_msg_seq++,
                             SS_CMD_ALARM_ON, NULL, 0);
            ss_mesh_send(&g_mesh, &cmd);

        } else if (level_pct >= SUMP_CRITICAL_LEVEL_PCT) {
            ss_message_t alert;
            uint8_t data[2] = { (uint8_t)(water_level & 0xFF),
                                (uint8_t)(water_level >> 8) };
            ss_build_alert(&alert, g_mesh.node_id, g_msg_seq++,
                           SS_ALERT_HIGH_WATER, 2, data, 2);
            ss_mesh_send(&g_mesh, &alert);
            ESP_LOGW(TAG, "WARNING: Water level %d%%", level_pct);

        } else if (pump_status == 0 && level_pct > 40 && water_level >
                   (g_storm_mode ? 300 : 500)) {
            /* Pump not running but water rising → possible pump failure */
            ss_message_t alert;
            ss_build_alert(&alert, g_mesh.node_id, g_msg_seq++,
                           SS_ALERT_PUMP_FAULT, 3, NULL, 0);
            ss_mesh_send(&g_mesh, &alert);
            ESP_LOGE(TAG, "PUMP FAULT: Water rising, pump not running!");

            /* Activate backup pump */
            ss_message_t cmd;
            ss_build_command(&cmd, g_mesh.node_id, 0xFF, g_msg_seq++,
                             SS_CMD_PUMP_ON, NULL, 0);
            ss_mesh_send(&g_mesh, &cmd);
        }

        if (!mains) {
            /* Power outage alert */
            ss_message_t alert;
            uint8_t data[1] = { bat_v };
            ss_build_alert(&alert, g_mesh.node_id, g_msg_seq++,
                           SS_ALERT_POWER_OUTAGE, 2, data, 1);
            ss_mesh_send(&g_mesh, &alert);
        }

        /* Build and send telemetry */
        ss_message_t msg;
        ss_build_sump_telem(&msg, g_mesh.node_id, g_msg_seq++,
                            bat_v, water_level, pump_current, pump_status,
                            flow_rate, water_temp, vib_rms, vib_peak,
                            mains, g_pump_runtime_today,
                            g_mesh.last_rssi);

        ss_mesh_wait_slot(&g_mesh);
        ss_mesh_send(&g_mesh, &msg);

        /* Check for incoming commands (storm mode, etc.) */
        ss_message_t rx_msg;
        if (ss_mesh_recv(&g_mesh, &rx_msg, 500) == 0) {
            if (rx_msg.header.type == SS_MSG_FLOOD_STATUS) {
                g_storm_mode = (rx_msg.payload[0] >= 2) ? 1 : 0;
                ESP_LOGI(TAG, "Flood status: level=%d score=%d storm=%d",
                         rx_msg.payload[0], rx_msg.payload[1], g_storm_mode);
            } else if (rx_msg.header.type == SS_MSG_COMMAND) {
                if (rx_msg.payload[0] == SS_CMD_STORM_MODE) {
                    g_storm_mode = 1;
                    ESP_LOGI(TAG, "Storm mode activated");
                } else if (rx_msg.payload[0] == SS_CMD_NORMAL_MODE) {
                    g_storm_mode = 0;
                    ESP_LOGI(TAG, "Normal mode restored");
                }
            }
        }

        /* Sleep: 30s normal, 15s storm mode */
        vTaskDelay(pdMS_TO_TICKS(g_storm_mode ?
                                 SUMP_SAMPLE_INTERVAL_STORM * 1000 :
                                 SUMP_SAMPLE_INTERVAL * 1000));
    }
}

/* === Main === */
void app_main(void)
{
    ESP_LOGI(TAG, "StormSync Sump Sentinel starting...");

    /* Initialize GPIOs */
    gpio_set_direction(SUMP_GPIO_SX_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(SUMP_GPIO_SX_RST, 1);
    gpio_set_direction(SUMP_GPIO_SX_DIO1, GPIO_MODE_INPUT);
    gpio_set_direction(SUMP_GPIO_US_TRIG, GPIO_MODE_OUTPUT);
    gpio_set_direction(SUMP_GPIO_US_ECHO, GPIO_MODE_INPUT);
    gpio_set_direction(SUMP_GPIO_ADXL_CS, GPIO_MODE_OUTPUT);
    gpio_set_level(SUMP_GPIO_ADXL_CS, 1);
    gpio_set_direction(SUMP_GPIO_PUMP_LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(SUMP_GPIO_MAINS, GPIO_MODE_INPUT);
    gpio_set_direction(SUMP_GPIO_FLOW, GPIO_MODE_INPUT);

    /* Initialize ADC */
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11); /* CT clamp */
    adc1_config_channel_atten(ADC1_CHANNEL_5, ADC_ATTEN_DB_11); /* Battery */

    /* Initialize SPI for SX1262 */
    spi_init();

    /* Initialize SPI for ADXL355 (separate bus or shared) */
    /* In production: initialize second SPI bus or use same with CS */

    /* Initialize radio + mesh */
    ss_radio_config_t radio_cfg = {
        .frequency = SS_NET_FREQ_HZ,
        .bandwidth = SS_NET_BW_HZ,
        .spreading_factor = SS_NET_SF,
        .coding_rate = SS_NET_CR,
        .preamble_len = SS_NET_PREAMBLE,
        .tx_power_dbm = SS_NET_TX_POWER_DBM,
        .rx_timeout_ms = 0,
    };

    ss_mesh_init(&g_mesh, SS_NODE_SUMP, &g_spi_iface, &radio_cfg);

    /* Join network */
    if (ss_mesh_join(&g_mesh) != 0) {
        ESP_LOGW(TAG, "Mesh join failed, will retry in measurement loop");
    }

    /* Start sensor task */
    xTaskCreate(sensor_task, "sensor", 8192, NULL, 5, NULL);

    ESP_LOGI(TAG, "Sump Sentinel running. Free heap: %lu",
             (unsigned long)esp_get_free_heap_size());
}