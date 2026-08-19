/*
 * CycleGuard Smart Light Set Firmware
 * Target: ESP32-C6-MINI-1
 *
 * Adaptive headlight (Luxeon 1000lm) + rear brake/turn signals (WS2812B×8).
 * Auto-dims on oncoming traffic (APDS9301 ambient light), brake detection
 * via ICM-42688-P IMU, turn signals from Hub commands.
 * BLE 5.0 to Hub.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "driver/rmt.h"

#include "../common/protocol.h"

static const char *TAG = "SMART_LIGHT";

/* ---- Pin definitions ---- */
#define PIN_I2C_SDA       1
#define PIN_I2C_SCL       2
#define PIN_SPI_CS_IMU    3
#define PIN_SPI_SCK       4
#define PIN_SPI_MISO      5
#define PIN_SPI_MOSI      6
#define PIN_IMU_INT       7
#define PIN_LED_FRONT_PWM 8
#define PIN_LED_REAR_DIN  9
#define PIN_CHG_STAT      10
#define PIN_BAT_SENSE     11
#define PIN_BTN_TURN_L    12
#define PIN_BTN_TURN_R    13
#define PIN_BTN_HAZARD    14
#define PIN_STATUS_LED    15

/* ---- Lighting constants ---- */
#define HEADLIGHT_MAX_PCT     100
#define HEADLIGHT_DAY_PCT     50
#define HEADLIGHT_NIGHT_PCT   80
#define HEADLIGHT_DIM_PCT     30   /* dimmed for oncoming traffic */
#define TAILLIGHT_DAY_PCT     50
#define TAILLIGHT_NIGHT_PCT   70
#define BRAKE_FLASH_HZ        4
#define TURN_CHASE_INTERVAL_MS 150
#define NUM_REAR_LEDS         8

/* ---- Brake detection ---- */
#define BRAKE_DECEL_G         0.3f
#define BRAKE_DURATION_MS     200
#define SAMPLE_RATE_HZ        100

/* ---- ICM-42688-P registers ---- */
#define IMU_REG_PWR_MGMT0   0x4C
#define IMU_REG_ACCEL_CONFIG0 0x50
#define IMU_REG_ACCEL_DATA  0x1F

/* ---- Light state ---- */
static uint8_t  g_headlight_pct   = HEADLIGHT_DAY_PCT;
static uint8_t  g_taillight_pct   = TAILLIGHT_DAY_PCT;
static bool     g_braking         = false;
static uint8_t  g_turn_signal     = TURN_NONE;
static uint8_t  g_mode            = 0;  /* 0=normal, 1=hazard, 2=crash, 3=DRL */
static float    g_ambient_lux     = 0.0f;
static uint8_t  g_battery_pct     = 100;

/* ---- Brake detection state ---- */
static float    g_forward_accel   = 0.0f;
static uint32_t g_brake_start_ms  = 0;
static bool     g_brake_armed     = false;

/* ---- SPI for IMU ---- */
static spi_device_handle_t g_imu_spi;

static void imu_spi_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_SPI_MOSI,
        .miso_io_num = PIN_SPI_MISO,
        .sclk_io_num = PIN_SPI_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 64,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, 1);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 10 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = PIN_SPI_CS_IMU,
        .queue_size = 4,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &g_imu_spi);
}

static void imu_read_reg(uint8_t reg, uint8_t *data, uint8_t len)
{
    spi_transaction_t t = {0};
    uint8_t tx = reg | 0x80;
    t.tx_buffer = &tx;
    t.rx_buffer = data;
    t.length = (1 + len) * 8;
    spi_device_polling_transmit(g_imu_spi, &t);
}

static void imu_init(void)
{
    /* Configure ICM-42688-P: accel ±8g, 100 Hz ODR */
    spi_transaction_t t = {0};
    uint8_t tx[2] = {IMU_REG_PWR_MGMT0 & 0x7F, 0x0F};
    t.tx_buffer = tx;
    t.length = 16;
    spi_device_polling_transmit(g_imu_spi, &t);

    tx[0] = IMU_REG_ACCEL_CONFIG0 & 0x7F;
    tx[1] = 0x06;  /* ±8g, 100 Hz */
    t.tx_buffer = tx;
    spi_device_polling_transmit(g_imu_spi, &t);

    ESP_LOGI(TAG, "ICM-42688-P initialized @ 100 Hz (brake detection)");
}

static float imu_read_forward_accel(void)
{
    /* Read forward-axis acceleration (Y-axis on light PCB) */
    uint8_t buf[6];
    imu_read_reg(IMU_REG_ACCEL_DATA, buf, 6);
    int16_t ay_raw = (buf[2] << 8) | buf[3];
    const float scale = 8.0f / 32768.0f;
    return ay_raw * scale;
}

/* ---- APDS9301 ambient light sensor ---- */
static void apds9301_init(void)
{
    /* I²C init already done in main */
    /* APDS9300: power on, set integration time 101ms, gain x1 */
    uint8_t cmd[2] = {0x80, 0x03};  /* Power on */
    /* i2c_write(APDS9301_ADDR, cmd, 2); */
    ESP_LOGI(TAG, "APDS9301 ambient light sensor initialized");
}

static float apds9301_read_lux(void)
{
    /* In production: read channel 0+1, compute lux per datasheet */
    /* Simplified: return simulated value */
    return 250.0f;  /* daytime overcast */
}

/* ---- WS2812B rear LED driver (RMT) ---- */
static void ws2812_init(void)
{
    rmt_config_t cfg = RMT_DEFAULT_CONFIG_TX(PIN_LED_REAR_DIN, RMT_CHANNEL_0);
    cfg.clk_div = 2;
    rmt_config(&cfg);
    rmt_driver_install(cfg.channel, 0, 0);
    ESP_LOGI(TAG, "WS2812B rear LED strip initialized (%d LEDs)", NUM_REAR_LEDS);
}

static void ws2812_set_led(int idx, uint8_t r, uint8_t g, uint8_t b)
{
    /* In production: encode as RMT pulses (800 kHz Manchester) */
    /* Simplified: log */
    (void)idx; (void)r; (void)g; (void)b;
}

static void ws2812_update_all(uint8_t r, uint8_t g, uint8_t b)
{
    for (int i = 0; i < NUM_REAR_LEDS; i++) {
        ws2812_set_led(i, r, g, b);
    }
}

/* ---- Headlight PWM (LEDC) ---- */
static void headlight_pwm_init(void)
{
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
        .gpio_num = PIN_LED_FRONT_PWM,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0,
        .hpoint = 0,
    };
    ledc_channel_config(&ch_cfg);
    ESP_LOGI(TAG, "Headlight PWM initialized (LEDC, 1 kHz, 10-bit)");
}

static void headlight_set_pct(uint8_t pct)
{
    uint32_t duty = (pct * 1023) / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

/* ---- Light mode update ---- */
static void update_lights(void)
{
    /* Auto headlight based on ambient light */
    if (g_ambient_lux < 50.0f) {
        /* Night mode */
        if (g_headlight_pct != HEADLIGHT_DIM_PCT) {
            g_headlight_pct = HEADLIGHT_NIGHT_PCT;
            g_taillight_pct = TAILLIGHT_NIGHT_PCT;
        }
    } else if (g_ambient_lux > 1000.0f) {
        /* Day mode (DRL) */
        g_headlight_pct = HEADLIGHT_DAY_PCT;
        g_taillight_pct = TAILLIGHT_DAY_PCT;
    } else {
        /* Dusk/dawn */
        g_headlight_pct = 60;
        g_taillight_pct = 60;
    }

    /* Auto-dim on oncoming headlights (detected via APDS9301 > 500 lux spike) */
    if (g_ambient_lux > 500.0f && g_ambient_lux < 2000.0f) {
        g_headlight_pct = HEADLIGHT_DIM_PCT;
    }

    /* Mode overrides */
    if (g_mode == 1) {  /* hazard */
        headlight_set_pct(g_headlight_pct);
        /* Rear: both sides amber flash 2 Hz */
        static bool toggle = false;
        toggle = !toggle;
        if (toggle) {
            ws2812_update_all(255, 100, 0);  /* amber */
        } else {
            ws2812_update_all(0, 0, 0);
        }
    } else if (g_mode == 2) {  /* crash mode */
        static bool toggle = false;
        toggle = !toggle;
        if (toggle) {
            headlight_set_pct(100);  /* strobe full power */
            ws2812_update_all(255, 0, 0);  /* red */
        } else {
            headlight_set_pct(0);
            ws2812_update_all(255, 255, 255);  /* white */
        }
    } else if (g_braking) {
        /* Brake mode: rear bright red flash, front unchanged */
        headlight_set_pct(g_headlight_pct);
        static bool toggle = false;
        toggle = !toggle;
        if (toggle) {
            ws2812_update_all(255, 0, 0);  /* bright red */
        } else {
            ws2812_update_all(100, 0, 0);  /* dim red */
        }
    } else if (g_turn_signal == TURN_LEFT) {
        /* Left turn: amber chase on left 4 LEDs, red on right */
        headlight_set_pct(g_headlight_pct);
        static int chase = 0;
        for (int i = 0; i < NUM_REAR_LEDS; i++) {
            if (i < 4) {  /* left side */
                if (i == chase % 4) {
                    ws2812_set_led(i, 255, 100, 0);  /* amber chasing */
                } else {
                    ws2812_set_led(i, 0, 0, 0);
                }
            } else {  /* right side steady red */
                ws2812_set_led(i, 80, 0, 0);
            }
        }
        chase++;
    } else if (g_turn_signal == TURN_RIGHT) {
        /* Right turn: amber chase on right 4 LEDs, red on left */
        headlight_set_pct(g_headlight_pct);
        static int chase = 0;
        for (int i = 0; i < NUM_REAR_LEDS; i++) {
            if (i >= 4) {  /* right side */
                if (i == 4 + (chase % 4)) {
                    ws2812_set_led(i, 255, 100, 0);
                } else {
                    ws2812_set_led(i, 0, 0, 0);
                }
            } else {  /* left side steady red */
                ws2812_set_led(i, 80, 0, 0);
            }
        }
        chase++;
    } else {
        /* Normal: steady tail light */
        headlight_set_pct(g_headlight_pct);
        uint8_t tail_r = (g_taillight_pct * 120) / 100;
        ws2812_update_all(tail_r, 0, 0);
    }
}

/* ---- Brake detection task ---- */
static void brake_detect_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));  /* 100 Hz */

        g_forward_accel = imu_read_forward_accel();

        /* Brake: deceleration > 0.3g (negative forward acceleration) */
        if (g_forward_accel < -BRAKE_DECEL_G) {
            if (!g_brake_armed) {
                g_brake_armed = true;
                g_brake_start_ms = esp_timer_get_time() / 1000;
            } else {
                uint32_t elapsed = (esp_timer_get_time() / 1000) - g_brake_start_ms;
                if (elapsed > BRAKE_DURATION_MS && !g_braking) {
                    g_braking = true;
                    ESP_LOGI(TAG, "BRAKING detected: %.2fg deceleration", -g_forward_accel);
                }
            }
        } else {
            /* Release brake */
            if (g_braking) {
                g_braking = false;
                ESP_LOGI(TAG, "Brake released");
            }
            g_brake_armed = false;
        }
    }
}

/* ---- Light update task (50 Hz for smooth animation) ---- */
static void light_update_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(20));  /* 50 Hz */

        /* Read ambient light (slower rate — every 200ms) */
        static int light_counter = 0;
        if (light_counter++ % 10 == 0) {
            g_ambient_lux = apds9301_read_lux();
        }

        update_lights();
    }
}

/* ---- Turn signal button handlers ---- */
static void IRAM_ATTR btn_turn_l_handler(void *arg)
{
    g_turn_signal = (g_turn_signal == TURN_LEFT) ? TURN_NONE : TURN_LEFT;
    g_mode = 0;
    ESP_LOGI(TAG, "Turn signal: %s", g_turn_signal == TURN_LEFT ? "LEFT" : "OFF");
}

static void IRAM_ATTR btn_turn_r_handler(void *arg)
{
    g_turn_signal = (g_turn_signal == TURN_RIGHT) ? TURN_NONE : TURN_RIGHT;
    g_mode = 0;
    ESP_LOGI(TAG, "Turn signal: %s", g_turn_signal == TURN_RIGHT ? "RIGHT" : "OFF");
}

static void IRAM_ATTR btn_hazard_handler(void *arg)
{
    g_mode = (g_mode == 1) ? 0 : 1;  /* toggle hazard */
    if (g_mode == 1) g_turn_signal = TURN_NONE;
    ESP_LOGI(TAG, "Hazard mode: %s", g_mode == 1 ? "ON" : "OFF");
}

/* ---- BLE command handler (from Hub) ---- */
static void handle_light_cmd(const light_cmd_payload_t *cmd)
{
    if (cmd->turn_signal != 0xFF) {  /* 0xFF = no change */
        g_turn_signal = cmd->turn_signal;
    }
    if (cmd->headlight_pct > 0) {
        g_headlight_pct = cmd->headlight_pct;
    }
    if (cmd->mode != 0xFF) {
        g_mode = cmd->mode;
        if (g_mode == 2) {  /* crash mode from Hub */
            ESP_LOGE(TAG, "CRASH MODE activated by Hub");
        }
    }
}

/* ---- I²C init ---- */
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

/* ---- Main ---- */
void app_main(void)
{
    ESP_LOGI(TAG, "CycleGuard Smart Light starting...");

    /* GPIO */
    gpio_set_direction(PIN_BTN_TURN_L, GPIO_MODE_INPUT);
    gpio_set_direction(PIN_BTN_TURN_R, GPIO_MODE_INPUT);
    gpio_set_direction(PIN_BTN_HAZARD, GPIO_MODE_INPUT);
    gpio_pullup_en(PIN_BTN_TURN_L);
    gpio_pullup_en(PIN_BTN_TURN_R);
    gpio_pullup_en(PIN_BTN_HAZARD);
    gpio_set_intr_type(PIN_BTN_TURN_L, GPIO_INTR_NEGEDGE);
    gpio_set_intr_type(PIN_BTN_TURN_R, GPIO_INTR_NEGEDGE);
    gpio_set_intr_type(PIN_BTN_HAZARD, GPIO_INTR_NEGEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_BTN_TURN_L, btn_turn_l_handler, NULL);
    gpio_isr_handler_add(PIN_BTN_TURN_R, btn_turn_r_handler, NULL);
    gpio_isr_handler_add(PIN_BTN_HAZARD, btn_hazard_handler, NULL);

    /* Peripherals */
    i2c_init();
    imu_spi_init();
    imu_init();
    apds9301_init();
    ws2812_init();
    headlight_pwm_init();

    /* Tasks */
    xTaskCreate(brake_detect_task, "brake_det", 4096, NULL, 5, NULL);
    xTaskCreate(light_update_task, "light_upd", 4096, NULL, 4, NULL);

    ESP_LOGI(TAG, "CycleGuard Smart Light ready — headlight + brake/turn signals active");

    /* Demo: simulate brake after 10s */
    vTaskDelay(pdMS_TO_TICKS(10000));
    ESP_LOGI(TAG, "Demo: simulating brake activation");
    g_braking = true;
    vTaskDelay(pdMS_TO_TICKS(3000));
    g_braking = false;
}