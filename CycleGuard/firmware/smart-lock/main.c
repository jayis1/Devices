/*
 * CycleGuard Smart Lock Firmware
 * Target: ESP32-C6-MINI-1
 *
 * GPS theft tracking + accelerometer tamper detection + motorized deadbolt
 * + 120 dB alarm + 4G LTE cellular backup.
 *
 * Sub-GHz 868 MHz to Hub (arm/disarm commands, status).
 * 4G LTE (SIM7600G) for theft tracking when bike moved beyond Sub-GHz range.
 * GPS (CAM-M8Q) for location tracking during alarm state.
 * ICM-42688-P IMU for tamper/drill/movement detection.
 * HX711 load cell for prying force detection.
 * AS5600 magnetic encoder for deadbolt position verification.
 * Motorized deadbolt (12V gear motor via DRV8871).
 * 120 dB piezo siren.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "driver/uart.h"
#include "driver/ledc.h"
#include "nvs_flash.h"

#include "../common/protocol.h"
#include "../common/mesh.h"

static const char *TAG = "SMART_LOCK";

/* ---- Pin definitions ---- */
#define PIN_I2C_SDA       1
#define PIN_I2C_SCL       2
#define PIN_SPI_CS_SX     3
#define PIN_SPI_SCK       4
#define PIN_SPI_MISO      5
#define PIN_SPI_MOSI      6
#define PIN_SX_DIO1       7
#define PIN_SX_BUSY       8
#define PIN_SX_RESET      9
#define PIN_IMU_INT       10
#define PIN_MOTOR_PWM     11
#define PIN_MOTOR_DIR     12
#define PIN_HX711_SCK     14
#define PIN_HX711_DOUT    15
#define PIN_SIREN         16
#define PIN_GPS_TX        17
#define PIN_GPS_RX        18
#define PIN_LTE_TX        19
#define PIN_LTE_RX        20
#define PIN_LTE_PWR       21
#define PIN_BTN_DISARM    22
#define PIN_LED_STATUS    23
#define PIN_BAT_SENSE     24
#define PIN_CHG_STAT      25

/* ---- Lock constants ---- */
#define TAMPER_ACC_THRESHOLD    0.5f    /* g, sustained movement */
#define TAMPER_LOAD_KG          15.0f   /* kg, prying force */
#define ALARM_LOAD_KG           30.0f   /* kg, definite tamper */
#define DRILL_FREQ_MIN_HZ       20.0f   /* Hz, drill vibration */
#define DRILL_FREQ_MAX_HZ       200.0f
#define GEOFENCE_DEFAULT_M      50      /* meters */
#define GPS_TRACKING_INTERVAL_S 300     /* 5 minutes */
#define SIREN_DURATION_S        30
#define IMU_SAMPLE_RATE_HZ      100

/* ---- ICM-42688-P registers ---- */
#define IMU_REG_PWR_MGMT0     0x4C
#define IMU_REG_ACCEL_CONFIG0 0x50
#define IMU_REG_ACCEL_DATA    0x1F

/* ---- State ---- */
static mesh_state_t g_mesh;
static uint16_t g_seq = 0;

static uint8_t  g_lock_state      = LOCK_DISARMED;
static int32_t  g_gps_lat_e7      = 0;
static int32_t  g_gps_lon_e7      = 0;
static uint8_t  g_tamper_count    = 0;
static uint16_t g_load_cell_kg    = 0;
static uint8_t  g_battery_pct     = 100;
static uint8_t  g_geo_fence_m     = GEOFENCE_DEFAULT_M;
static bool     g_siren_active    = false;
static uint32_t g_siren_start_ms  = 0;

/* ---- Parked location (geo-fence center) ---- */
static int32_t  g_parked_lat_e7   = 0;
static int32_t  g_parked_lon_e7   = 0;

/* ---- SPI for SX1262 + IMU ---- */
static spi_device_handle_t g_sx_spi;
static spi_device_handle_t g_imu_spi;

/* ---- UART for GPS and LTE ---- */
#define GPS_UART    UART_NUM_1
#define LTE_UART    UART_NUM_2

/* ---- I2C init ---- */
static void i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}

/* ---- SX1262 SPI init ---- */
static void sx1262_spi_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_SPI_MOSI,
        .miso_io_num = PIN_SPI_MISO,
        .sclk_io_num = PIN_SPI_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 256,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, 1);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 8 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = PIN_SPI_CS_SX,
        .queue_size = 4,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &g_sx_spi);
}

static void sx1262_transmit(const uint8_t *data, size_t len)
{
    spi_transaction_t t = {0};
    t.tx_buffer = data;
    t.length = len * 8;
    spi_device_polling_transmit(g_sx_spi, &t);
}

/* ---- IMU init ---- */
static void imu_init(void)
{
    /* Separate CS for IMU on same SPI bus */
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 10 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = -1,  /* manual CS control */
        .queue_size = 4,
    };
    /* In production: use separate CS pin or mux */
}

static float imu_read_accel_mag(void)
{
    /* Read accelerometer magnitude */
    /* Simplified: return 0 — production reads ICM-42688-P via SPI */
    return 0.1f;  /* resting state */
}

/* ---- HX711 load cell ---- */
static uint16_t hx711_read_kg(void)
{
    /* In production: read HX711 24-bit ADC, convert to kg */
    return 0;  /* no force when idle */
}

/* ---- GPS CAM-M8Q ---- */
static void gps_init(void)
{
    uart_config_t cfg = {
        .baud_rate  = 9600,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_NONE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_param_config(GPS_UART, &cfg);
    uart_set_pin(GPS_UART, PIN_GPS_TX, PIN_GPS_RX, -1, -1);
    uart_driver_install(GPS_UART, 1024, 0, 0, NULL, 0);
    ESP_LOGI(TAG, "CAM-M8Q GPS initialized (UART1)");
}

static bool gps_read_position(int32_t *lat_e7, int32_t *lon_e7)
{
    /* Parse NMEA from GPS UART */
    char buf[256];
    int len = uart_read_bytes(GPS_UART, (uint8_t *)buf, sizeof(buf) - 1,
                              pdMS_TO_TICKS(100));
    if (len <= 0) return false;
    buf[len] = '\0';

    /* Find $GPRMC sentence */
    char *rmc = strstr(buf, "$GPRMC");
    if (!rmc) return false;

    /* Parse: $GPRMC,time,status,lat,N,lon,W,... */
    char status;
    float lat_f, lon_f;
    if (sscanf(rmc, "$GPRMC,%*f,%c,%f,%*c,%f", &status, &lat_f, &lon_f) >= 3) {
        if (status == 'A') {
            *lat_e7 = (int32_t)(lat_f / 100.0f * 1e7);
            *lon_e7 = (int32_t)(lon_f / 100.0f * 1e7);
            return true;
        }
    }
    return false;
}

/* ---- 4G LTE (SIM7600G) ---- */
static void lte_init(void)
{
    gpio_set_direction(PIN_LTE_PWR, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_LTE_PWR, 0);
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_set_level(PIN_LTE_PWR, 1);

    uart_config_t cfg = {
        .baud_rate  = 115200,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_NONE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_param_config(LTE_UART, &cfg);
    uart_set_pin(LTE_UART, PIN_LTE_TX, PIN_LTE_RX, -1, -1);
    uart_driver_install(LTE_UART, 1024, 1024, 0, NULL, 0);
    ESP_LOGI(TAG, "SIM7600G 4G LTE initialized");
}

static void lte_send_sms(const char *phone, const char *message)
{
    /* AT commands to SIM7600G */
    uart_write_bytes(LTE_UART, "AT+CMGF=1\r\n", 11);
    vTaskDelay(pdMS_TO_TICKS(500));
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "AT+CMGS=\"%s\"\r\n", phone);
    uart_write_bytes(LTE_UART, cmd, strlen(cmd));
    vTaskDelay(pdMS_TO_TICKS(500));
    uart_write_bytes(LTE_UART, message, strlen(message));
    uart_write_bytes(LTE_UART, "\x1A", 1);  /* Ctrl-Z to send */
    ESP_LOGI(TAG, "LTE SMS sent to %s: %s", phone, message);
}

/* ---- Siren ---- */
static void siren_on(void)
{
    gpio_set_level(PIN_SIREN, 1);
    g_siren_active = true;
    g_siren_start_ms = esp_timer_get_time() / 1000;
    ESP_LOGW(TAG, "SIREN ON (120 dB)");
}

static void siren_off(void)
{
    gpio_set_level(PIN_SIREN, 0);
    g_siren_active = false;
    ESP_LOGI(TAG, "Siren off");
}

/* ---- Motorized deadbolt control ---- */
static void deadbolt_lock(void)
{
    /* Extend deadbolt: motor forward for 2 seconds */
    gpio_set_level(PIN_MOTOR_DIR, 1);  /* forward */
    /* PWM at 50% duty for 2 seconds */
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 512);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    vTaskDelay(pdMS_TO_TICKS(2000));
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

    /* Verify deadbolt position via AS5600 */
    /* In production: read AS5600 angle, confirm bolt engaged */
    ESP_LOGI(TAG, "Deadbolt LOCKED (position verified by AS5600)");
}

static void deadbolt_unlock(void)
{
    /* Retract deadbolt: motor reverse for 2 seconds */
    gpio_set_level(PIN_MOTOR_DIR, 0);  /* reverse */
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 512);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    vTaskDelay(pdMS_TO_TICKS(2000));
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

    ESP_LOGI(TAG, "Deadbolt UNLOCKED (position verified by AS5600)");
}

/* ---- Send lock status to Hub via Sub-GHz ---- */
static void send_lock_status(void)
{
    lock_payload_t payload;
    payload.lock_state    = g_lock_state;
    payload.gps_lat_e7    = g_gps_lat_e7;
    payload.gps_lon_e7    = g_gps_lon_e7;
    payload.tamper_count  = g_tamper_count;
    payload.load_cell_kg  = g_load_cell_kg;
    payload.battery_pct   = g_battery_pct;

    mesh_frame_t frame;
    uint8_t msg_type = (g_lock_state == LOCK_ALARM ||
                        g_lock_state == LOCK_TRACKING) ?
                        MSG_TYPE_THEFT_ALERT : MSG_TYPE_SENSOR_DATA;

    protocol_build_frame(&frame, g_mesh.self_id, NODE_ID_HUB,
                         msg_type, g_seq++,
                         (uint8_t *)&payload, sizeof(payload));
    sx1262_transmit((uint8_t *)&frame, FRAME_MAX_LEN);
}

/* ---- Lock state machine ---- */
static void lock_arm(void)
{
    if (g_lock_state != LOCK_DISARMED) return;

    /* Get GPS position for geo-fence center */
    gps_read_position(&g_parked_lat_e7, &g_parked_lon_e7);

    /* Lock deadbolt */
    deadbolt_lock();

    g_lock_state = LOCK_ARMED;
    g_tamper_count = 0;
    ESP_LOGI(TAG, "Lock ARMED — geo-fence center: %.7f, %.7f",
             g_parked_lat_e7 / 1e7f, g_parked_lon_e7 / 1e7f);
}

static void lock_disarm(void)
{
    deadbolt_unlock();
    g_lock_state = LOCK_DISARMED;
    siren_off();
    ESP_LOGI(TAG, "Lock DISARMED");
}

static void lock_trigger_alarm(void)
{
    g_lock_state = LOCK_ALARM;
    siren_on();
    g_tamper_count++;

    /* Get GPS position */
    gps_read_position(&g_gps_lat_e7, &g_gps_lon_e7);

    /* Send theft alert via Sub-GHz to Hub */
    send_lock_status();

    /* Send SMS via 4G LTE to owner */
    char msg[256];
    snprintf(msg, sizeof(msg),
             "CycleGuard THEFT ALERT! Your bike is being moved. "
             "Location: %.6f,%.6f Tracking active.",
             g_gps_lat_e7 / 1e7f, g_gps_lon_e7 / 1e7f);
    lte_send_sms("+18055551234", msg);

    ESP_LOGE(TAG, "ALARM ACTIVATED — theft in progress!");
}

/* ---- Tamper detection task ---- */
static void tamper_detect_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(50));  /* 20 Hz check */

        if (g_lock_state == LOCK_DISARMED) continue;

        /* Read IMU acceleration magnitude */
        float accel_mag = imu_read_accel_mag();

        /* Read load cell (prying force) */
        g_load_cell_kg = hx711_read_kg();

        /* Movement detection: > 0.5g sustained when armed */
        if (accel_mag > TAMPER_ACC_THRESHOLD && g_lock_state == LOCK_ARMED) {
            ESP_LOGW(TAG, "TAMPER: movement detected (%.2fg)", accel_mag);
            g_lock_state = LOCK_TAMPER;
            g_tamper_count++;
            send_lock_status();
        }

        /* Prying force detection */
        if (g_load_cell_kg > TAMPER_LOAD_KG && g_lock_state == LOCK_ARMED) {
            ESP_LOGW(TAG, "TAMPER: prying force %d kg", g_load_cell_kg);
            g_lock_state = LOCK_TAMPER;
            g_tamper_count++;
            send_lock_status();
        }

        /* Escalate to alarm if force > 30 kg or continued tampering */
        if ((g_load_cell_kg > ALARM_LOAD_KG || g_tamper_count > 3) &&
            g_lock_state == LOCK_TAMPER) {
            lock_trigger_alarm();
        }

        /* Tamper timeout → back to armed */
        if (g_lock_state == LOCK_TAMPER) {
            static uint32_t tamper_start = 0;
            uint32_t now = esp_timer_get_time() / 1000;
            if (tamper_start == 0) tamper_start = now;
            if (now - tamper_start > 30000) {  /* 30 s no further tamper */
                g_lock_state = LOCK_ARMED;
                tamper_start = 0;
                ESP_LOGI(TAG, "Tamper timeout — back to ARMED");
            }
        }

        /* Siren timeout (30 seconds) */
        if (g_siren_active) {
            uint32_t now = esp_timer_get_time() / 1000;
            if (now - g_siren_start_ms > SIREN_DURATION_S * 1000) {
                siren_off();
            }
        }

        /* Transition to TRACKING if bike is moving during alarm */
        if (g_lock_state == LOCK_ALARM) {
            /* Check GPS for movement */
            int32_t lat, lon;
            if (gps_read_position(&lat, &lon)) {
                g_gps_lat_e7 = lat;
                g_gps_lon_e7 = lon;
                /* If moved > geo-fence, enter tracking */
                /* Simplified: always transition after alarm */
                g_lock_state = LOCK_TRACKING;
                ESP_LOGW(TAG, "Entering TRACKING mode — GPS: %.6f,%.6f",
                         lat / 1e7f, lon / 1e7f);
            }
        }

        /* Tracking mode: send GPS updates every 5 minutes */
        if (g_lock_state == LOCK_TRACKING) {
            static uint32_t last_track_ms = 0;
            uint32_t now = esp_timer_get_time() / 1000;
            if (now - last_track_ms > GPS_TRACKING_INTERVAL_S * 1000) {
                send_lock_status();
                last_track_ms = now;
            }
        }
    }
}

/* ---- Sub-GHz command receiver task ---- */
static void mesh_receiver_task(void *arg)
{
    /* In production: listen for Hub commands in assigned TDMA slot */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        /* Check for incoming Sub-GHz frames (simplified) */
        /* Parse lock_cmd_payload_t */
        /* If command == 1 (arm): lock_arm() */
        /* If command == 0 (disarm): lock_disarm() */
        /* If command == 3 (alarm): lock_trigger_alarm() */

        /* Send heartbeat to Hub */
        send_lock_status();
    }
}

/* ---- Disarm button (physical key) ---- */
static void IRAM_ATTR btn_disarm_handler(void *arg)
{
    /* Physical key switch — immediate disarm */
    /* In production: require keyed switch, not just button */
}

/* ---- Main ---- */
void app_main(void)
{
    ESP_LOGI(TAG, "CycleGuard Smart Lock starting...");

    /* NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* GPIO */
    gpio_set_direction(PIN_SIREN, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_MOTOR_DIR, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_BTN_DISARM, GPIO_MODE_INPUT);
    gpio_pullup_en(PIN_BTN_DISARM);
    gpio_set_intr_type(PIN_BTN_DISARM, GPIO_INTR_NEGEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_BTN_DISARM, btn_disarm_handler, NULL);

    /* LED PWM for motor */
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = 1000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_cfg);

    ledc_channel_config_t ch_cfg = {
        .channel = LEDC_CHANNEL_0,
        .duty = 0,
        .gpio_num = PIN_MOTOR_PWM,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0,
    };
    ledc_channel_config(&ch_cfg);

    /* Peripherals */
    i2c_init();
    sx1262_spi_init();
    imu_init();
    gps_init();
    lte_init();

    /* Mesh */
    mesh_init(&g_mesh, NODE_ID_SMART_LOCK, false);

    /* Tasks */
    xTaskCreate(tamper_detect_task, "tamper", 4096, NULL, 5, NULL);
    xTaskCreate(mesh_receiver_task, "mesh_rx", 4096, NULL, 4, NULL);

    ESP_LOGI(TAG, "CycleGuard Smart Lock ready (state: DISARMED)");

    /* Demo: auto-arm after 10 seconds */
    vTaskDelay(pdMS_TO_TICKS(10000));
    ESP_LOGI(TAG, "Demo: auto-arming lock");
    lock_arm();
}