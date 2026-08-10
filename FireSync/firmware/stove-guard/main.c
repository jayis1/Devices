/*
 * FireSync — Stove Guard Firmware
 * ESP32-S3, FreeRTOS
 *
 * Watches the stovetop via MLX90640 thermal array, tracks burner knobs
 * via AS5600 magnetic encoders (TCA9548A mux), and auto-shuts off the
 * gas supply on: unattended cooking (30 min timer), pan overheating,
 * oil smoking, or flame detection. Motorized ball valve fails CLOSED.
 *
 * Build: idf.py build with ESP-IDF v5.x
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"

#include "../common/protocol.h"
#include "../common/subghz_mesh.h"
#include "../common/config.h"

static const char *TAG = "FireSync-Stove";

/* === Sensor Context === */
typedef struct {
    int16_t  thermal_max_x10;   /* Max pan temp ×0.1°C */
    int16_t  thermal_mean_x10;  /* Mean temp ×0.1°C */
    uint8_t  knob_positions;    /* Bitmask: bits 0-3 on/off, bits 4-7 level */
    uint8_t  pantemp_class;     /* PanTemp CNN output 0-3 */
    uint16_t timer_remaining_s; /* Auto-shutoff timer */
    uint8_t  valve_state;       /* 0=open, 1=closed */
    uint8_t  buzzer_active;
    uint8_t  battery_v;
    uint32_t last_presence_ms;  /* Last PIR/occupancy signal from sentinel */
    uint8_t  any_burner_on;    /* Derived from knob positions */
} fs_stove_ctx_t;

static fs_stove_ctx_t g_stove;
static fs_mesh_ctx_t g_mesh;
static fs_radio_hal_t g_radio_hal;
static fs_radio_config_t g_radio_cfg;
static SemaphoreHandle_t g_stove_mutex;
static spi_device_handle_t g_spi;
static SemaphoreHandle_t g_spi_mutex;

/* === SX1262 HAL === */
static int hal_spi_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = STOVE_GPIO_SX_MOSI,
        .miso_io_num = STOVE_GPIO_SX_MISO,
        .sclk_io_num = STOVE_GPIO_SX_SCK,
        .max_transfer_sz = 256,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 2000000, .mode = 0,
        .spics_io_num = -1, .queue_size = 4,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &g_spi);
    g_spi_mutex = xSemaphoreCreateMutex();
    return 0;
}

static int hal_spi_xfer(const uint8_t *tx, uint8_t *rx, size_t len)
{
    xSemaphoreTake(g_spi_mutex, portMAX_DELAY);
    spi_transaction_t t = {0};
    t.length = len * 8;
    if (tx) t.tx_buffer = tx;
    if (rx) t.rx_buffer = rx;
    spi_device_polling_transmit(g_spi, &t);
    xSemaphoreGive(g_spi_mutex);
    return 0;
}

static void hal_cs_low(void)  { gpio_set_level(STOVE_GPIO_SX_NSS, 0); }
static void hal_cs_high(void) { gpio_set_level(STOVE_GPIO_SX_NSS, 1); }
static void hal_reset(int a) { gpio_set_level(STOVE_GPIO_SX_RST, !a); }
static int  hal_dio1_read(void) { return gpio_get_level(STOVE_GPIO_SX_DIO1); }
static int  hal_busy_read(void) { return gpio_get_level(STOVE_GPIO_SX_BUSY); }
static void hal_delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
static void hal_delay_us(uint32_t us) { esp_rom_delay_us(us); }
static void hal_on_dio1(void) {}

/* === TCA9548A I²C Mux (for 4× AS5600 knob encoders) === */
static int tca9548a_select(uint8_t channel)
{
    /* Production: I²C write to TCA9548A (0x70) with channel bitmask */
    uint8_t cmd = 1 << channel;
    /* i2c_master_write_to_device(I2C_NUM_0, 0x70, &cmd, 1, 100); */
    return 0;
}

/* === AS5600 Magnetic Encoder (per burner knob) === */
static int as5600_read(uint8_t channel, uint16_t *angle)
{
    /* Production: TCA9548A select → I²C read AS5600 (0x36) raw angle reg 0x0C-0x0D */
    tca9548a_select(channel);
    *angle = 0; /* Placeholder: 0 = knob off, 0-1023 = rotation */
    return 0;
}

static uint8_t read_knob_positions(void)
{
    /* Read 4 burner knobs, return bitmask */
    uint8_t mask = 0;
    uint16_t angles[4];
    for (int i = 0; i < 4; i++) {
        as5600_read(i, &angles[i]);
        /* If angle > threshold, burner is on */
        if (angles[i] > 50) {
            mask |= (1 << i); /* on/off bit */
            /* Level: 0-3 based on angle range */
            uint8_t level = (angles[i] > 700) ? 3 : (angles[i] > 400) ? 2 : 1;
            mask |= (level << (4 + i * 2)); /* Level in bits 4-7 */
        }
    }
    return mask;
}

/* === MLX90640 Thermal Array === */
#define MLX90640_ROWS  24
#define MLX90640_COLS  32
static int16_t g_thermal_frame[MLX90640_ROWS * MLX90640_COLS];

static int mlx90640_read_stove(int16_t *max_x10, int16_t *mean_x10)
{
    /* Production: I²C read MLX90640 at 400 kHz */
    /* Mounted under range hood looking down at stovetop */
    int16_t max_val = 0;
    int32_t sum = 0;
    for (int i = 0; i < MLX90640_ROWS * MLX90640_COLS; i++) {
        g_thermal_frame[i] = 280 + (i % 5); /* ~28°C ambient placeholder */
        sum += g_thermal_frame[i];
        if (g_thermal_frame[i] > max_val) max_val = g_thermal_frame[i];
    }
    *max_x10 = max_val;
    *mean_x10 = (int16_t)(sum / (MLX90640_ROWS * MLX90640_COLS));
    return 0;
}

/* === PanTemp CNN === */
static int pantemp_infer(int16_t thermal_max_x10, uint8_t *class_out)
{
    /* Production: TFLite-Micro int8 model
     * Input: 32×24 thermal snapshot
     * Output: 4-class (safe_cooking, overheating, oil_smoking, flaming)
     */
    int16_t temp_c = thermal_max_x10 / 10;

    if (temp_c >= FS_PAN_TEMP_OIL_IGNITE_C) {
        *class_out = FS_PAN_FLAMING;
    } else if (temp_c >= FS_PAN_TEMP_OIL_SMOKING_C) {
        *class_out = FS_PAN_OIL_SMOKING;
    } else if (temp_c > 200) {
        *class_out = FS_PAN_OVERHEATING;
    } else {
        *class_out = FS_PAN_SAFE_COOKING;
    }
    return 0;
}

/* === Gas Valve Control (L298N H-bridge) === */
static void valve_close(void)
{
    /* Close valve: energize close direction + spring return */
    gpio_set_level(STOVE_GPIO_VALVE_CLOSE, 1);
    vTaskDelay(pdMS_TO_TICKS(2000)); /* 2s to close */
    gpio_set_level(STOVE_GPIO_VALVE_CLOSE, 0);
    g_stove.valve_state = 1;

    /* Verify with reed switch */
    if (gpio_get_level(STOVE_GPIO_VALVE_FB) == 0) {
        ESP_LOGI(TAG, "Gas valve closed (confirmed by reed switch)");
    } else {
        ESP_LOGW(TAG, "Gas valve close: reed switch not confirming!");
    }
}

static void valve_open(void)
{
    /* Open valve: energize open direction */
    gpio_set_level(STOVE_GPIO_VALVE_OPEN, 1);
    vTaskDelay(pdMS_TO_TICKS(2000));
    gpio_set_level(STOVE_GPIO_VALVE_OPEN, 0);
    g_stove.valve_state = 0;
    ESP_LOGI(TAG, "Gas valve opened");
}

/* === Fire Alert === */
static void send_fire_alert(uint8_t fire_class, uint8_t confidence)
{
    fs_message_t msg;
    fs_build_fire_alert(&msg, g_mesh.node_id, g_mesh.msg_counter++,
                       fire_class, confidence, 0xFF, /* Room 0xFF = stove */
                       0, 0, g_stove.thermal_max_x10,
                       g_stove.thermal_max_x10, 0);
    fs_mesh_send_emergency(&g_mesh, &g_radio_hal, &msg);
    ESP_LOGW(TAG, "FIRE ALERT: class=%d conf=%d%% thermal=%.1f°C",
             fire_class, confidence, g_stove.thermal_max_x10 / 10.0);
}

/* === Stove Monitor Task === */
static void stove_task(void *arg)
{
    memset(&g_stove, 0, sizeof(g_stove));
    g_stove.valve_state = 0; /* Open by default */
    g_stove.battery_v = 420;

    while (1) {
        xSemaphoreTake(g_stove_mutex, portMAX_DELAY);

        /* Read thermal array */
        mlx90640_read_stove(&g_stove.thermal_max_x10,
                             &g_stove.thermal_mean_x10);

        /* Read knob positions */
        g_stove.knob_positions = read_knob_positions();
        g_stove.any_burner_on = (g_stove.knob_positions & 0x0F) != 0;

        /* PanTemp CNN */
        pantemp_infer(g_stove.thermal_max_x10, &g_stove.pantemp_class);

        /* Auto-shutoff timer */
        if (g_stove.any_burner_on && g_stove.timer_remaining_s > 0) {
            g_stove.timer_remaining_s--;
            if (g_stove.timer_remaining_s == 0) {
                ESP_LOGW(TAG, "Unattended cooking timer expired — closing gas valve!");
                valve_close();
                g_stove.buzzer_active = 1;
                send_fire_alert(FS_CLASS_SMOLDERING, 80);
            }
        } else if (g_stove.any_burner_on && g_stove.timer_remaining_s == 0) {
            /* Start timer when burner first turned on */
            g_stove.timer_remaining_s = FS_STOVE_UNATTENDED_S;
        }

        /* Thermal-based shutoff */
        if (g_stove.pantemp_class == FS_PAN_FLAMING) {
            ESP_LOGE(TAG, "FLAMING PAN DETECTED — closing gas valve!");
            valve_close();
            g_stove.buzzer_active = 1;
            send_fire_alert(FS_CLASS_FLAMING_FIRE, 95);
        } else if (g_stove.pantemp_class == FS_PAN_OIL_SMOKING) {
            ESP_LOGW(TAG, "Oil smoking detected — closing gas valve!");
            valve_close();
            g_stove.buzzer_active = 1;
            send_fire_alert(FS_CLASS_SMOLDERING, 85);
        }

        xSemaphoreGive(g_stove_mutex);

        /* Buzzer control */
        if (g_stove.buzzer_active) {
            ledc_set_duty(LEDC_LOW_SPEED_CHANNEL, LEDC_CHANNEL_0, 200);
        } else {
            ledc_set_duty(LEDC_LOW_SPEED_CHANNEL, LEDC_CHANNEL_0, 0);
        }
        ledc_update_duty(LEDC_LOW_SPEED_CHANNEL, LEDC_CHANNEL_0);

        vTaskDelay(pdMS_TO_TICKS(1000)); /* 1 Hz */
    }
}

/* === Telemetry Task === */
static void telemetry_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(STOVE_TELEM_INTERVAL_MS));

        xSemaphoreTake(g_stove_mutex, portMAX_DELAY);
        fs_stove_telem_t telem;
        memset(&telem, 0, sizeof(telem));
        telem.subtype          = FS_TELEM_STOVE;
        telem.battery_v        = g_stove.battery_v;
        telem.thermal_max_x10  = g_stove.thermal_max_x10;
        telem.thermal_mean_x10 = g_stove.thermal_mean_x10;
        telem.knob_positions   = g_stove.knob_positions;
        telem.pantemp_class    = g_stove.pantemp_class;
        telem.timer_remaining_s= g_stove.timer_remaining_s;
        telem.valve_state      = g_stove.valve_state;
        telem.buzzer_active    = g_stove.buzzer_active;
        telem.free_heap        = (uint16_t)esp_get_free_heap_size();
        telem.rssi             = g_mesh.last_rssi;
        telem.uptime_min       = 0;
        xSemaphoreGive(g_stove_mutex);

        fs_message_t msg;
        fs_build_stove_telem(&msg, g_mesh.node_id, g_mesh.msg_counter++, &telem);
        fs_mesh_send(&g_mesh, &g_radio_hal, &msg, 0);
    }
}

/* === Radio RX Task === */
static void radio_rx_task(void *arg)
{
    uint8_t rx_buf[FS_MAX_MSG];
    fs_message_t msg;

    while (1) {
        int rx_len = fs_sx1262_rx(&g_radio_hal, rx_buf, sizeof(rx_buf),
                                   4000, &g_mesh.last_rssi);

        if (rx_len > 0 && fs_decode(&msg, rx_buf, rx_len) == 0) {
            switch (msg.header.type) {
            case FS_MSG_STOVE_SHUTOFF:
            case FS_MSG_COMMAND:
                if (msg.header.type == FS_MSG_COMMAND &&
                    msg.payload[0] == FS_CMD_CLOSE_VALVE) {
                    ESP_LOGW(TAG, "Hub commanded gas valve close!");
                    xSemaphoreTake(g_stove_mutex, portMAX_DELAY);
                    valve_close();
                    xSemaphoreGive(g_stove_mutex);
                } else if (msg.header.type == FS_MSG_STOVE_SHUTOFF) {
                    ESP_LOGW(TAG, "Stove shutoff from Hub (fire response)");
                    xSemaphoreTake(g_stove_mutex, portMAX_DELAY);
                    valve_close();
                    xSemaphoreGive(g_stove_mutex);
                }
                break;

            case FS_MSG_ALARM_TRIGGER:
                g_stove.buzzer_active = 1;
                break;

            case FS_MSG_ALARM_STOP:
                g_stove.buzzer_active = 0;
                break;
            }
        }
    }
}

/* === Main === */
void app_main(void)
{
    ESP_LOGI(TAG, "FireSync Stove Guard starting...");

    /* GPIO */
    gpio_set_direction(STOVE_GPIO_SX_NSS, GPIO_MODE_OUTPUT);
    gpio_set_direction(STOVE_GPIO_SX_RST, GPIO_MODE_OUTPUT);
    gpio_set_direction(STOVE_GPIO_SX_DIO1, GPIO_MODE_INPUT);
    gpio_set_direction(STOVE_GPIO_SX_BUSY, GPIO_MODE_INPUT);
    gpio_set_direction(STOVE_GPIO_VALVE_OPEN, GPIO_MODE_OUTPUT);
    gpio_set_direction(STOVE_GPIO_VALVE_CLOSE, GPIO_MODE_OUTPUT);
    gpio_set_direction(STOVE_GPIO_VALVE_FB, GPIO_MODE_INPUT);
    gpio_set_direction(STOVE_GPIO_LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(STOVE_GPIO_BUZZER, GPIO_MODE_OUTPUT);
    gpio_set_direction(STOVE_GPIO_VBAT, GPIO_MODE_ANALOG);
    gpio_set_direction(STOVE_GPIO_USB_PWR, GPIO_MODE_INPUT);

    gpio_set_level(STOVE_GPIO_SX_NSS, 1);
    gpio_set_level(STOVE_GPIO_SX_RST, 1);
    gpio_set_level(STOVE_GPIO_VALVE_OPEN, 0);
    gpio_set_level(STOVE_GPIO_VALVE_CLOSE, 0);

    /* I²C for TCA9548A + MLX90640 */
    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = STOVE_GPIO_KNOB_SDA,
        .scl_io_num = STOVE_GPIO_KNOB_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    i2c_param_config(I2C_NUM_0, &i2c_cfg);
    i2c_driver_install(I2C_NUM_0, i2c_cfg.mode, 0, 0, 0);

    /* Buzzer PWM */
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = 3000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_cfg);
    ledc_channel_config_t ch_cfg = {
        .channel = LEDC_CHANNEL_0,
        .duty = 0, .gpio_num = STOVE_GPIO_BUZZER,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0, .hpoint = 0,
    };
    ledc_channel_config(&ch_cfg);

    /* Radio */
    g_radio_hal.spi_init   = hal_spi_init;
    g_radio_hal.spi_xfer   = hal_spi_xfer;
    g_radio_hal.cs_low     = hal_cs_low;
    g_radio_hal.cs_high    = hal_cs_high;
    g_radio_hal.reset      = hal_reset;
    g_radio_hal.dio1_read  = hal_dio1_read;
    g_radio_hal.busy_read = hal_busy_read;
    g_radio_hal.delay_ms  = hal_delay_ms;
    g_radio_hal.delay_us  = hal_delay_us;
    g_radio_hal.on_dio1   = hal_on_dio1;

    g_radio_cfg.freq_hz         = FS_SUBGHZ_FREQ_HZ;
    g_radio_cfg.spreading_factor= FS_SUBGHZ_SF;
    g_radio_cfg.bandwidth_hz    = FS_SUBGHZ_BW_HZ;
    g_radio_cfg.tx_power_dbm    = FS_SUBGHZ_TX_POWER_DBM;
    g_radio_cfg.sync_word       = FS_SYNC_WORD;
    g_radio_cfg.preamble_len    = FS_SUBGHZ_PREAMBLE;

    g_radio_hal.spi_init();
    fs_sx1262_init(&g_radio_hal, &g_radio_cfg);

    /* Mesh */
    uint8_t aes_key[16] = {0};
    fs_mesh_init(&g_mesh, 0x10, FS_NODE_STOVE, aes_key);
    g_mesh.battery_v = 420;

    if (fs_mesh_join(&g_mesh, &g_radio_hal) == 0) {
        ESP_LOGI(TAG, "Joined mesh, slot %d", g_mesh.tdma_slot);
    } else {
        ESP_LOGW(TAG, "Mesh join failed — retrying");
    }

    g_stove_mutex = xSemaphoreCreateMutex();

    xTaskCreate(stove_task, "stove", 8192, NULL, 5, NULL);
    xTaskCreate(telemetry_task, "telem", 4096, NULL, 3, NULL);
    xTaskCreate(radio_rx_task, "radio_rx", 4096, NULL, 4, NULL);

    ESP_LOGI(TAG, "Stove Guard ready. Valve=%s, monitoring stovetop.",
             g_stove.valve_state ? "CLOSED" : "OPEN");
}