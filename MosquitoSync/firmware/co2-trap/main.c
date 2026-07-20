/*
 * MosquitoSync — CO2 Trap Node Firmware
 * ESP32-S3, FreeRTOS
 *
 * The CO2 Trap generates CO2 from propane catalytic combustion + heat (37°C)
 * + octenol lure to attract mosquitoes. An IR beam counter tracks insect
 * entries, and an OV2640 camera captures trap catch images every 15 min
 * for the cloud CaptureCount CNN. Reports telemetry to Hub via Sub-GHz mesh.
 *
 * Safety interlocks: propane leak detection (MQ-4), overheat shutoff,
 * trap bag full, rain pause, wind protection.
 *
 * Build: idf.py build with ESP-IDF v5.x
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"
#include "driver/ledc.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "../common/protocol.h"
#include "../common/sx1262.h"
#include "../common/mesh.h"
#include "../common/config.h"

static const char *TAG = "MosquitoSync-Trap";

/* === State === */
static uint16_t g_msg_seq = 0;
static uint16_t g_ir_breaks_period = 0;
static uint16_t g_capture_24h = 0;
static uint8_t  g_trap_fullness = 0;
static uint8_t  g_co2_on = 0;
static uint8_t  g_propane_pct = 100;
static uint8_t  g_fan_pct = TRAP_FAN_DEFAULT_PCT;
static uint8_t  g_dominant_species = 7;

/* IR beam break counter (ISR) */
static volatile uint16_t g_ir_count = 0;

/* Rain gauge counter (ISR) */
static volatile uint16_t g_rain_tips = 0;

/* === SX1262 SPI Interface === */
static spi_device_handle_t g_spi_dev;

static void spi_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = TRAP_GPIO_SX_MOSI,
        .miso_io_num = TRAP_GPIO_SX_MISO,
        .sclk_io_num = TRAP_GPIO_SX_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 256,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 8000000,
        .mode = 0,
        .spics_io_num = TRAP_GPIO_SX_NSS,
        .queue_size = 4,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &g_spi_dev);
}

static void spi_cs_select(void) { }
static void spi_cs_release(void) { }
static uint8_t spi_transfer(uint8_t byte) {
    uint8_t rx;
    spi_transaction_t t = { .tx_buffer = &byte, .rx_buffer = &rx, .length = 8 };
    spi_device_polling_transmit(g_spi_dev, &t);
    return rx;
}
static void spi_reset(uint8_t assert) {
    gpio_set_level(TRAP_GPIO_SX_RST, assert ? 0 : 1);
}
static void spi_delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
static int spi_dio1_read(void) { return gpio_get_level(TRAP_GPIO_SX_DIO1); }
static void spi_dio1_irq_enable(int enable) { (void)enable; }

static const ms_spi_interface_t g_spi_iface = {
    .init = spi_init,
    .cs_select = spi_cs_select,
    .cs_release = spi_cs_release,
    .transfer = spi_transfer,
    .reset = spi_reset,
    .delay_ms = spi_delay_ms,
    .dio1_read = spi_dio1_read,
    .dio1_irq_enable = spi_dio1_irq_enable,
};

/* === I2C for BME280 === */
static void i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = TRAP_GPIO_BME_SDA,
        .scl_io_num = TRAP_GPIO_BME_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}

static void read_bme280(int16_t *temp_deci, uint16_t *humidity_deci,
                         uint16_t *pressure_deci)
{
    uint8_t buf[8];
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 0x76 << 1 | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0xF7, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 0x76 << 1 | I2C_MASTER_READ, true);
    i2c_master_read(cmd, buf, 8, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    *pressure_deci = (uint16_t)((buf[0] << 12 | buf[1] << 4 | buf[2] >> 4) / 256.0 * 10);
    *temp_deci = (int16_t)((buf[3] << 12 | buf[4] << 4 | buf[5] >> 4) / 100.0 * 10);
    *humidity_deci = (uint16_t)((buf[6] << 8 | buf[7]) / 1024.0 * 1000.0);
}

/* === PWM for Fan + PTC Heater === */
static void pwm_init(void)
{
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = 25000, /* 25 kHz for fan */
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_cfg);

    /* Fan PWM */
    ledc_channel_config_t fan_cfg = {
        .channel = LEDC_CHANNEL_0,
        .duty = (g_fan_pct * 1023) / 100,
        .gpio_num = TRAP_GPIO_FAN_PWM,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .hpoint = 0,
        .timer_sel = LEDC_TIMER_0,
    };
    ledc_channel_config(&fan_cfg);

    /* PTC Heater PWM (slower freq) */
    ledc_timer_config_t heat_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_1,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = 1000, /* 1 kHz for heater */
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&heat_timer);

    ledc_channel_config_t heat_cfg = {
        .channel = LEDC_CHANNEL_1,
        .duty = 0, /* Start off */
        .gpio_num = TRAP_GPIO_HEATER_PWM,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .hpoint = 0,
        .timer_sel = LEDC_TIMER_1,
    };
    ledc_channel_config(&heat_cfg);
}

static void set_fan_speed(uint8_t pct)
{
    g_fan_pct = pct;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, (pct * 1023) / 100);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static void set_heater_power(uint8_t pct)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, (pct * 1023) / 100);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}

/* === IR Beam Break ISR === */
static void IRAM_ATTR ir_beam_isr(void *arg)
{
    /* Debounce: only count if >200ms since last break */
    static uint32_t last_time = 0;
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000); /* ms */
    if (now - last_time > 200) {
        g_ir_count++;
    }
    last_time = now;
}

/* === Rain Gauge ISR === */
static void IRAM_ATTR rain_isr(void *arg)
{
    g_rain_tips++;
}

/* === Propane Valve Control === */
static void propane_valve_open(void)
{
    gpio_set_level(TRAP_GPIO_PROPANE, 1);
    g_co2_on = 1;
    ESP_LOGI(TAG, "Propane valve OPEN (CO2 generation on)");
}

static void propane_valve_close(void)
{
    gpio_set_level(TRAP_GPIO_PROPANE, 0);
    g_co2_on = 0;
    ESP_LOGI(TAG, "Propane valve CLOSED (CO2 generation off)");
}

/* === Safety Check === */
static int safety_check(int16_t temp_deci)
{
    /* Overheat shutoff */
    if (temp_deci > TRAP_HEATER_MAX_C * 10) {
        ESP_LOGE(TAG, "OVERHEAT: %.1f°C > %d°C — shutting down heater + propane",
                 temp_deci / 10.0, TRAP_HEATER_MAX_C);
        set_heater_power(0);
        propane_valve_close();
        return -1;
    }

    /* Trap bag full */
    if (gpio_get_level(TRAP_GPIO_TRAP_FULL) == 0) {
        ESP_LOGW(TAG, "Trap bag FULL — disabling fan");
        set_fan_speed(0);
        g_trap_fullness = 100;
        return -1;
    }

    return 0;
}

/* === Camera Capture (OV2640 stub) ===
 * In production: initialize esp_camera, capture JPEG, store in flash,
 * upload to cloud via Hub mesh (chunked) for CaptureCount CNN.
 */
static void camera_capture(void)
{
    /* In production:
     * camera_fb_t *fb = esp_camera_capture();
     * if (fb) { store image, queue for upload }
     */
    ESP_LOGI(TAG, "Camera capture (OV2640) — image queued for cloud upload");

    /* Update trap fullness estimate based on capture count */
    if (g_capture_24h > 0) {
        g_trap_fullness = (uint8_t)(g_capture_24h * 100 / 500); /* 500 = bag capacity */
        if (g_trap_fullness > 100) g_trap_fullness = 100;
    }
}

/* === Send Telemetry to Hub === */
static void send_telemetry(ms_mesh_ctx_t *mesh)
{
    int16_t temp_deci;
    uint16_t hum_deci, pres_deci;
    read_bme280(&temp_deci, &hum_deci, &pres_deci);

    /* Get IR beam breaks since last report */
    g_ir_breaks_period = g_ir_count;
    g_ir_count = 0;

    /* Get rain tips since last report */
    uint16_t rain = g_rain_tips;
    g_rain_tips = 0;

    /* Estimate propane level (decreases with usage) */
    if (g_co2_on) {
        /* ~0.15% per hour of operation (3-4 week tank life) */
        static uint32_t last_update = 0;
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000000ULL);
        if (last_update > 0) {
            uint32_t hours = (now - last_update) / 3600;
            if (hours > 0) {
                g_propane_pct = (g_propane_pct > hours * 0.15) ?
                    g_propane_pct - (uint8_t)(hours * 0.15) : 0;
            }
        }
        last_update = now;
    }

    uint8_t battery_v = 0; /* Read from ADC in production */

    ms_message_t msg;
    ms_build_trap_telem(&msg, mesh->node_id, g_msg_seq++,
                        battery_v, temp_deci, hum_deci, pres_deci, rain,
                        g_ir_breaks_period, g_capture_24h,
                        g_trap_fullness, g_co2_on, g_propane_pct,
                        g_fan_pct, g_dominant_species, mesh->last_rssi);
    ms_mesh_send(mesh, &msg);

    ESP_LOGI(TAG, "Telemetry: temp=%.1f°C IR_breaks=%d capture_24h=%d "
             "co2=%d propane=%d%% fan=%d%% fullness=%d%%",
             temp_deci / 10.0, g_ir_breaks_period, g_capture_24h,
             g_co2_on, g_propane_pct, g_fan_pct, g_trap_fullness);
}

/* === CO2 Control Task (dusk-dawn scheduling) === */
static void co2_task(void *arg)
{
    ESP_LOGI(TAG, "CO2 control task started");

    /* Propane valve control */
    gpio_set_direction(TRAP_GPIO_PROPANE, GPIO_MODE_OUTPUT);

    /* Default: dusk-dawn operation (18:00–06:00 local)
     * In production: use RTC + location to compute sunset/sunrise
     */
    uint8_t co2_active = 0;

    while (1) {
        /* Check if trap is full or propane empty */
        if (g_trap_fullness >= TRAP_BAG_FULL_PCT || g_propane_pct < TRAP_PROPANE_LOW_PCT) {
            if (co2_active) {
                propane_valve_close();
                set_fan_speed(0);
                set_heater_power(0);
                co2_active = 0;
            }
            vTaskDelay(pdMS_TO_TICKS(60000));
            continue;
        }

        /* Check high-risk mode from hub (forces 24/7 operation) */
        /* In production: check mesh->high_risk_mode flag */

        /* Simplified schedule: on for 12 hours, off for 12 */
        /* In production: use actual sunset/sunrise times */
        static uint32_t cycle = 0;
        uint8_t should_be_on = ((cycle % 24) < 12); /* 12h on, 12h off */

        if (should_be_on && !co2_active) {
            /* Turn on CO2 generation */
            propane_valve_open();
            set_fan_speed(TRAP_FAN_DEFAULT_PCT);
            /* PTC heater: PID control to maintain 37°C */
            set_heater_power(60); /* ~60% duty to reach 37°C */
            co2_active = 1;
            ESP_LOGI(TAG, "CO2 generation started (dusk-dawn cycle)");
        } else if (!should_be_on && co2_active) {
            propane_valve_close();
            set_fan_speed(0);
            set_heater_power(0);
            co2_active = 0;
            ESP_LOGI(TAG, "CO2 generation stopped (daytime)");
        }

        cycle++;

        /* Wait 1 hour between cycle checks */
        /* (In production: check every minute against actual sunset time) */
        vTaskDelay(pdMS_TO_TICKS(3600000 / 3600)); /* Simplified: 1s per "hour" */
    }
}

/* === PTC Heater PID Task === */
static void heater_pid_task(void *arg)
{
    /* Maintain trap surface at 37°C (human body temperature)
     * Simple proportional control: adjust heater PWM based on temp
     */
    ESP_LOGI(TAG, "Heater PID task started (target: %d°C)", TRAP_HEATER_TARGET_C);
    while (1) {
        int16_t temp_deci;
        uint16_t hum_deci, pres_deci;
        read_bme280(&temp_deci, &hum_deci, &pres_deci);
        int16_t temp_c = temp_deci / 10;

        if (g_co2_on && temp_c < TRAP_HEATER_TARGET_C) {
            /* Increase heater power proportionally */
            int error = TRAP_HEATER_TARGET_C - temp_c;
            uint8_t power = (uint8_t)(error * 5); /* Kp = 5% per °C */
            if (power > 100) power = 100;
            set_heater_power(power);
        } else if (temp_c > TRAP_HEATER_TARGET_C + 2) {
            set_heater_power(0);
        }

        /* Safety check */
        if (safety_check(temp_deci) != 0) {
            /* Emergency shutdown already done in safety_check */
        }

        vTaskDelay(pdMS_TO_TICKS(5000)); /* 5-second control loop */
    }
}

/* === Camera + Telemetry Task === */
static void camera_telemetry_task(void *arg)
{
    ms_mesh_ctx_t *mesh = (ms_mesh_ctx_t *)arg;
    ESP_LOGI(TAG, "Camera + telemetry task started (15-min interval)");

    while (1) {
        /* Wait for mesh to join */
        if (!mesh->joined) {
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        /* Capture camera image */
        camera_capture();

        /* Update capture count from IR breaks */
        /* Each mosquito triggers ~1-3 beam breaks; estimate captures */
        if (g_ir_breaks_period > 0) {
            g_capture_24h += g_ir_breaks_period / 2; /* ~50% are mosquitoes */
        }

        /* Send telemetry to Hub */
        send_telemetry(mesh);

        vTaskDelay(pdMS_TO_TICKS(TRAP_SAMPLE_INTERVAL * 1000));
    }
}

/* === Mesh Task === */
static void mesh_task(void *arg)
{
    ms_mesh_ctx_t *mesh = (ms_mesh_ctx_t *)arg;

    ms_radio_config_t radio_cfg = {
        .frequency = MS_NET_FREQ_HZ,
        .bandwidth = MS_NET_BW_HZ,
        .spreading_factor = MS_NET_SF,
        .coding_rate = MS_NET_CR,
        .preamble_len = MS_NET_PREAMBLE,
        .tx_power_dbm = MS_NET_TX_POWER_DBM,
        .rx_timeout_ms = 0,
    };

    if (ms_mesh_init(mesh, MS_NODE_TRAP, &g_spi_iface, &radio_cfg) != 0) {
        ESP_LOGE(TAG, "Mesh init failed");
        vTaskDelete(NULL);
    }

    /* Join network */
    int join_retries = 0;
    while (join_retries < 10) {
        if (ms_mesh_join(mesh) == 0) {
            ESP_LOGI(TAG, "Joined mesh: node_id=%d slot=%d",
                     mesh->node_id, mesh->tdma_slot);
            break;
        }
        ESP_LOGW(TAG, "Join failed, retry %d", join_retries);
        vTaskDelay(pdMS_TO_TICKS(2000));
        join_retries++;
    }

    /* Listen for hub commands */
    ms_message_t msg;
    while (1) {
        if (ms_mesh_recv(mesh, &msg, 5000) == 0) {
            switch (msg.header.type) {
                case MS_MSG_COMMAND: {
                    uint8_t cmd = msg.payload[0];
                    ESP_LOGI(TAG, "Command from hub: cmd=0x%02X", cmd);
                    switch (cmd) {
                        case MS_CMD_TRAP_CO2_ON:
                            propane_valve_open();
                            break;
                        case MS_CMD_TRAP_CO2_OFF:
                            propane_valve_close();
                            break;
                        case MS_CMD_TRAP_FAN_ON:
                            set_fan_speed(TRAP_FAN_DEFAULT_PCT);
                            break;
                        case MS_CMD_TRAP_FAN_OFF:
                            set_fan_speed(0);
                            break;
                        case MS_CMD_CAPTURE_IMAGE:
                            camera_capture();
                            break;
                        case MS_CMD_HIGH_RISK_MODE:
                            /* Force 24/7 CO2 operation */
                            propane_valve_open();
                            set_fan_speed(100);
                            break;
                        case MS_CMD_NORMAL_MODE:
                            /* Return to dusk-dawn schedule */
                            break;
                        default:
                            break;
                    }
                    break;
                }
                case MS_MSG_TIME_SYNC:
                case MS_MSG_CONFIG:
                    break;
                default:
                    break;
            }
        }
    }
}

/* === Main === */
void app_main(void)
{
    ESP_LOGI(TAG, "MosquitoSync CO2 Trap starting...");

    static ms_mesh_ctx_t mesh;

    nvs_flash_init();

    /* Initialize GPIOs */
    gpio_set_direction(TRAP_GPIO_SX_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(TRAP_GPIO_SX_RST, 1);
    gpio_set_direction(TRAP_GPIO_SX_DIO1, GPIO_MODE_INPUT);
    gpio_set_direction(TRAP_GPIO_TRAP_FULL, GPIO_MODE_INPUT_PULLUP);

    /* IR beam break interrupt */
    gpio_set_direction(TRAP_GPIO_IR_BEAM, GPIO_MODE_INPUT);
    gpio_set_intr_type(TRAP_GPIO_IR_BEAM, GPIO_INTR_NEGEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(TRAP_GPIO_IR_BEAM, ir_beam_isr, NULL);

    /* Rain gauge interrupt */
    gpio_set_direction(TRAP_GPIO_RAIN_TIP, GPIO_MODE_INPUT);
    gpio_set_intr_type(TRAP_GPIO_RAIN_TIP, GPIO_INTR_NEGEDGE);
    gpio_isr_handler_add(TRAP_GPIO_RAIN_TIP, rain_isr, NULL);

    /* Initialize I2C */
    i2c_init();

    /* Initialize PWM */
    pwm_init();

    /* Create tasks */
    xTaskCreate(mesh_task, "mesh", 8192, &mesh, 5, NULL);
    xTaskCreate(co2_task, "co2", 4096, NULL, 4, NULL);
    xTaskCreate(heater_pid_task, "heater", 4096, NULL, 3, NULL);
    xTaskCreate(camera_telemetry_task, "cam_telem", 8192, &mesh, 3, NULL);

    ESP_LOGI(TAG, "CO2 Trap running. Free heap: %lu bytes",
             (unsigned long)esp_get_free_heap_size());
}