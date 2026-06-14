/*
 * hub_main.c — SleepSync Nightstand Hub (ESP32-S3-WROOM-1-N8R8)
 *
 * Responsibilities:
 * - BLE 5 mesh provisioner (configures all nodes)
 * - Real-time sleep staging from strip data (TFLite Micro)
 * - Adaptive soundscape audio engine (I2S DAC)
 * - Smart alarm with optimal wake timing
 * - Environment control (sends setpoints to climate/shade nodes)
 * - TFT display rendering (sleep stage, environment, clock)
 * - WiFi uplink to MQTT broker
 * - BLE GATT server for mobile app
 * - OTA update distribution to all nodes
 *
 * FreeRTOS tasks:
 *   Task 1: BLE mesh management + GATT server
 *   Task 2: Sleep staging + alarm logic
 *   Task 3: Audio engine
 *   Task 4: Display rendering
 *   Task 5: WiFi/MQTT bridge
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/i2s.h"
#include "driver/i2c.h"
#include "driver/spi.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_wifi.h"
#include "esp_mqtt.h"
#include "nvs_flash.h"

#include "mesh_protocol.h"

/* ---- Pin Definitions ---- */
#define PIN_I2S_BCLK    0
#define PIN_I2S_WS      1
#define PIN_I2S_DOUT    2
#define PIN_I2S_DIN     3
#define PIN_I2S_MCLK    4
#define PIN_I2C0_SDA   6
#define PIN_I2C0_SCL    7
#define PIN_SPI_CLK     8
#define PIN_SPI_MOSI    9
#define PIN_SPI_MISO    10
#define PIN_TFT_CS      11
#define PIN_TFT_DC      12
#define PIN_TFT_RST     13
#define PIN_TFT_BL      14
#define PIN_SD_CS       15
#define PIN_WS2812B     18
#define BTN_SNOOZE      19
#define BTN_MODE        20
#define BTN_BRIGHT      21
#define PIN_BATT_SENSE  38
#define PIN_CHG_STATUS  39

/* ---- Sleep Staging Model Constants ---- */
#define SLEEP_WINDOW_SIZE    60   /* 60 samples × 5s = 5 min window */
#define SLEEP_NUM_FEATURES   11   /* features per sample from sleep_data_payload_t */
#define SLEEP_STAGING_INTERVAL_MS  5000

/* ---- Alarm State ---- */
typedef struct {
    bool     enabled;
    uint32_t window_start;  /* Unix timestamp */
    uint32_t window_end;    /* Unix timestamp */
    bool     triggered;
    uint8_t  snooze_count;
    uint32_t snooze_until;
} alarm_state_t;

/* ---- Environment Setpoints ---- */
typedef struct {
    float    temp_setpoint;    /* °C */
    float    hum_setpoint;     /* %RH */
    float    temp_deep_offset; /* offset for deep sleep (usually -1.0 to -2.0) */
    float    temp_rem_offset;  /* offset for REM (usually +0.5) */
    uint8_t  shade_position;  /* 0-100% during sleep (0 = closed) */
    uint8_t  noise_floor;     /* Base soundscape volume 0-255 */
} env_setpoints_t;

/* ---- Node State ---- */
#define MAX_MESH_NODES  6

typedef struct {
    uint8_t  node_id;
    uint8_t  node_type;    /* 0=unknown, 1=strip, 2=climate, 3=shade */
    bool     active;
    int64_t  last_seen_ms;
    union {
        sleep_data_payload_t  sleep;
        env_data_payload_t    env;
        shade_status_payload_t shade;
    } data;
    float    sleep_quality;  /* instantaneous quality metric 0-1 */
} node_state_t;

/* ---- Globals ---- */
static node_state_t nodes[MAX_MESH_NODES];
static alarm_state_t alarm = {0};
static env_setpoints_t setpoints = {
    .temp_setpoint = 19.0f,
    .hum_setpoint = 45.0f,
    .temp_deep_offset = -1.5f,
    .temp_rem_offset = 0.5f,
    .shade_position = 0,      /* closed during sleep */
    .noise_floor = 80,        /* moderate volume */
};

static uint8_t current_sleep_stage = STAGE_AWAKE;
static float   current_sleep_score = 0.0f;
static uint8_t soundscape_id = 1;  /* 0=off, 1=white, 2=pink, 3=brown, 4=rain, 5=ocean */
static uint8_t soundscape_volume = 80;

/* Sleep history ring buffer for nightly tracking */
#define SLEEP_HISTORY_HOURS   12
#define SLEEP_HISTORY_SAMPLES (SLEEP_HISTORY_HOURS * 720)  /* 720 samples/hr at 5s */
static uint8_t sleep_stage_history[SLEEP_HISTORY_SAMPLES];
static uint16_t sleep_history_idx = 0;
static uint16_t sleep_history_count = 0;

/* ---- TFLite Micro Sleep Staging (stub) ---- */

static uint8_t run_sleep_staging(const sleep_data_payload_t *recent, uint16_t count)
{
    /*
     * In production: run TFLite Micro 1D-CNN+BiLSTM+Attention model
     * Input: [count × 11] feature matrix
     * Output: 4-class softmax (wake, light, deep, REM)
     *
     * Stub: rule-based classifier for demonstration
     */
    float hr = recent[count - 1].heart_rate / 10.0f;
    float rr = recent[count - 1].resp_rate / 10.0f;
    uint8_t move = recent[count - 1].movement;
    uint8_t snore = recent[count - 1].snoring;

    /* Simple heuristic rules (clinical-inspired) */
    if (move > 100) return STAGE_AWAKE;
    if (move > 40)  return STAGE_LIGHT;

    /* Deep sleep: low HR, low movement, regular breathing */
    if (move < 10 && hr < 58.0f && rr < 14.0f && snore < 30)
        return STAGE_DEEP;

    /* REM: moderate HR variability, low movement, irregular breathing */
    if (move < 15 && recent[count - 1].hrv > 50)
        return STAGE_REM;

    return STAGE_LIGHT;
}

/* ---- Smart Alarm Logic ---- */

static bool should_trigger_alarm(void)
{
    if (!alarm.enabled || alarm.triggered)
        return false;

    /* Get current time (stub — in production use NTP-synced RTC) */
    uint32_t now = 0; /* placeholder */

    if (now < alarm.window_start || now > alarm.window_end)
        return false;

    /* Check snooze */
    if (alarm.snooze_until > 0 && now < alarm.snooze_until)
        return false;

    /* Optimal wake: trigger in light sleep */
    if (current_sleep_stage == STAGE_LIGHT)
        return true;

    /* Acceptable: trigger in REM sleep (within 5 min of window end) */
    if (current_sleep_stage == STAGE_REM && (now > alarm.window_end - 300))
        return true;

    /* Hard alarm at end of window regardless */
    if (now >= alarm.window_end - 30)
        return true;

    /* Still in deep sleep — wait */
    return false;
}

static void trigger_alarm_sequence(void)
{
    alarm.triggered = true;

    /* 1. Start dawn simulation on shade controllers */
    hub_command_payload_t cmd = {
        .target_id = NODE_ID_SHADE_MIN,
        .cmd_type = CMD_SHADE_POSITION,
        .param_len = 1,
        .params = {100}  /* open shade fully */
    };
    uint8_t msg[64];
    uint16_t msg_len = mesh_build_message(
        NODE_ID_HUB, NODE_ID_SHADE_MIN, MSG_HUB_COMMAND,
        (uint8_t *)&cmd, sizeof(cmd), msg, sizeof(msg));

    /* 2. Fade in soundscape alarm (gentle → moderate over 60s) */
    /* In production: ramp audio volume from noise_floor to 200 over 60s */

    /* 3. Hub display shows morning briefing */
    printf("[ALARM] Smart alarm triggered at stage %d\n", current_sleep_stage);

    /* 4. Send alarm event to all nodes */
    uint8_t alarm_msg[16];
    mesh_build_message(NODE_ID_HUB, NODE_ID_BROADCAST,
                       MSG_ALARM_TRIGGER, alarm_msg, 1, msg, sizeof(msg));
}

static void handle_snooze(void)
{
    if (!alarm.triggered) return;
    alarm.snooze_count++;
    alarm.snooze_until = 0 + 300; /* now + 5 min (stub) */
    alarm.triggered = false;
    printf("[ALARM] Snoozed (%d times), next attempt in 5 min\n", alarm.snooze_count);
}

/* ---- Environment Optimization Logic ---- */

static void update_environment_setpoints(void)
{
    /* Adjust setpoints based on current sleep stage */
    float temp_target = setpoints.temp_setpoint;
    uint8_t vol_target = setpoints.noise_floor;

    switch (current_sleep_stage) {
    case STAGE_DEEP:
        temp_target += setpoints.temp_deep_offset;  /* cooler for deep sleep */
        vol_target = setpoints.noise_floor;           /* minimum noise masking */
        break;
    case STAGE_REM:
        temp_target += setpoints.temp_rem_offset;     /* slightly warmer for REM */
        vol_target = setpoints.noise_floor;           /* minimal disruption */
        break;
    case STAGE_LIGHT:
        temp_target += setpoints.temp_deep_offset * 0.5f; /* slightly cool */
        vol_target = setpoints.noise_floor + 20;      /* moderate masking */
        break;
    case STAGE_AWAKE:
    default:
        vol_target = setpoints.noise_floor + 40;      /* more masking if restless */
        break;
    }

    /* Send updated setpoint to climate node */
    hub_command_payload_t cmd = {
        .target_id = NODE_ID_CLIMATE,
        .cmd_type = CMD_CLIMATE_SETPOINT,
        .param_len = 4
    };
    int16_t temp_q = (int16_t)(temp_target * 100);
    uint16_t hum_q = (uint16_t)(setpoints.hum_setpoint * 100);
    memcpy(&cmd.params[0], &temp_q, 2);
    memcpy(&cmd.params[2], &hum_q, 2);

    uint8_t msg[64];
    mesh_build_message(NODE_ID_HUB, NODE_ID_CLIMATE, MSG_HUB_COMMAND,
                        (uint8_t *)&cmd, sizeof(cmd), msg, sizeof(msg));

    /* Adjust soundscape volume */
    soundscape_volume = vol_target;
}

/* ---- Sleep Score Calculation ---- */

static float compute_sleep_score(void)
{
    if (sleep_history_count == 0) return 0.0f;

    uint16_t total = sleep_history_count;
    uint16_t deep_count = 0, rem_count = 0, wake_count = 0;

    for (uint16_t i = 0; i < total; i++) {
        switch (sleep_stage_history[i]) {
        case STAGE_DEEP:  deep_count++; break;
        case STAGE_REM:   rem_count++; break;
        case STAGE_AWAKE: wake_count++; break;
        default: break;
        }
    }

    float deep_pct = (float)deep_count / total * 100.0f;
    float rem_pct = (float)rem_count / total * 100.0f;
    float wake_pct = (float)wake_count / total * 100.0f;

    /* Ideal: 15-20% deep, 20-25% REM, <5% wake */
    float score = 70.0f; /* base score */

    /* Deep sleep bonus/penalty */
    if (deep_pct >= 15.0f && deep_pct <= 25.0f)
        score += 15.0f;
    else if (deep_pct >= 10.0f)
        score += 8.0f;
    else
        score -= 5.0f;

    /* REM sleep bonus/penalty */
    if (rem_pct >= 20.0f && rem_pct <= 30.0f)
        score += 10.0f;
    else if (rem_pct >= 15.0f)
        score += 5.0f;
    else
        score -= 5.0f;

    /* Wake penalty */
    if (wake_pct < 5.0f)
        score += 5.0f;
    else if (wake_pct > 15.0f)
        score -= 10.0f;

    if (score > 100.0f) score = 100.0f;
    if (score < 0.0f) score = 0.0f;
    return score;
}

/* ---- BLE Mesh Provisioning (stub) ---- */

static void ble_mesh_init(void)
{
    /*
     * In production: initialize ESP-BLE-MESH stack
     * - Register provisioning callback
     * - Register vendor model
     * - Start advertising as unprovisioned device
     * - On provision: become provisioner, invite other nodes
     */
    printf("[BLE_MESH] Initialized as provisioner\n");
}

static void ble_mesh_process_rx(const uint8_t *data, uint16_t len)
{
    uint8_t src, dst, type, payload[50], payload_len;

    int8_t ret = mesh_parse_message(data, len, &src, &dst, &type,
                                     payload, &payload_len);
    if (ret != 0) {
        printf("[BLE_MESH] Parse error %d\n", ret);
        return;
    }

    /* Only process messages addressed to us or broadcast */
    if (dst != NODE_ID_HUB && dst != NODE_ID_BROADCAST)
        return;

    int idx = -1;
    for (int i = 0; i < MAX_MESH_NODES; i++) {
        if (nodes[i].node_id == src) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        /* Auto-register new node */
        for (int i = 0; i < MAX_MESH_NODES; i++) {
            if (!nodes[i].active) {
                nodes[i].node_id = src;
                nodes[i].active = true;
                idx = i;
                break;
            }
        }
    }
    if (idx < 0) return;

    nodes[idx].last_seen_ms = 0; /* stub: use xTaskGetTickCount() */

    switch (type) {
    case MSG_SLEEP_DATA:
        memcpy(&nodes[idx].data.sleep, payload, sizeof(sleep_data_payload_t));
        nodes[idx].node_type = 1; /* strip */
        break;
    case MSG_ENV_DATA:
        memcpy(&nodes[idx].data.env, payload, sizeof(env_data_payload_t));
        nodes[idx].node_type = 2; /* climate */
        break;
    case MSG_SHADE_STATUS:
        memcpy(&nodes[idx].data.shade, payload, sizeof(shade_status_payload_t));
        nodes[idx].node_type = 3; /* shade */
        break;
    case MSG_ACK:
        printf("[HUB] ACK from node %d\n", src);
        break;
    default:
        break;
    }
}

/* ---- Audio Engine (stub) ---- */

static void audio_init(void)
{
    /*
     * In production: configure I2S to UDA1334A DAC
     * - I2S_MODE_MASTER | I2S_MODE_TX
     * - 44100 Hz, 16-bit stereo
     * - Read PCM sound files from SD card
     * - Mix and apply volume/fade
     */
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,
        .sample_rate = 44100,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 1024,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0,
    };
    /* i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL); */
    printf("[AUDIO] I2S DAC initialized (UDA1334A)\n");
}

static void audio_play_soundscape(uint8_t sound_id, uint8_t volume)
{
    const char *sound_names[] = {
        "Off", "White Noise", "Pink Noise", "Brown Noise",
        "Rain", "Ocean Waves", "Forest", "Campfire"
    };
    if (sound_id > 7) sound_id = 1;
    printf("[AUDIO] Playing: %s, volume=%d/255\n",
           sound_names[sound_id], volume);
    /* In production: load PCM from SD, apply volume, write to I2S */
}

/* ---- I2C Sensors (stub) ---- */

static void sensors_init(void)
{
    /* BME280: temp/humidity on I2C0 */
    /* SCD40: CO2 on I2C0 */
    /* TSL2591: ambient light on I2C0 */
    /* SPH0645: I2S mic already configured */
    printf("[SENSORS] BME280 + SCD40 + TSL2591 initialized on I2C0\n");
}

static void read_hub_sensors(void)
{
    /* In production: read BME280, SCD40, TSL2591 via I2C */
    /* Used for hub's own ambient sensing and display */
}

/* ---- TFT Display (stub) ---- */

static void display_init(void)
{
    /* ILI9488 initialization via SPI */
    printf("[TFT] ILI9488 480×320 display initialized\n");
}

static void display_render(void)
{
    /* In production: draw sleep stage ring, environment gauges, clock */
    const char *stage_names[] = {"AWAKE", "LIGHT", "DEEP", "REM"};
    const char *stage_colors[] = {"RED", "BLUE", "PURPLE", "GREEN"};

    printf("[TFT] Sleep: %s | Score: %.0f | Temp: %.1f°C | Sound: %d\n",
           stage_names[current_sleep_stage],
           current_sleep_score,
           setpoints.temp_setpoint,
           soundscape_id);
}

/* ---- WiFi + MQTT (stub) ---- */

static void wifi_mqtt_init(void)
{
    /* In production: initialize esp_wifi + esp_mqtt_client */
    /* Connect to configured SSID, then connect to MQTT broker */
    printf("[WIFI] WiFi + MQTT initialized\n");
}

static void mqtt_publish_sleep_data(void)
{
    /* Publish latest sleep data to cloud via MQTT */
    /* Topic: sleepsync/{device_id}/sleep_data */
    printf("[MQTT] Publishing sleep data: stage=%d, score=%.0f\n",
           current_sleep_stage, current_sleep_score);
}

/* ---- Main Tasks ---- */

static void task_ble_mesh(void *arg)
{
    ble_mesh_init();
    while (1) {
        /* In production: process BLE mesh events from ESP-BLE-MESH stack */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void task_sleep_staging(void *arg)
{
    sleep_data_payload_t recent_window[SLEEP_WINDOW_SIZE];
    uint16_t window_idx = 0;

    while (1) {
        /* Collect sleep data from strip node */
        int strip_idx = -1;
        for (int i = 0; i < MAX_MESH_NODES; i++) {
            if (nodes[i].node_type == 1 && nodes[i].active) {
                strip_idx = i;
                break;
            }
        }

        if (strip_idx >= 0) {
            recent_window[window_idx % SLEEP_WINDOW_SIZE] =
                nodes[strip_idx].data.sleep;
            window_idx++;

            /* Run staging if we have enough data */
            uint16_t count = window_idx < SLEEP_WINDOW_SIZE ?
                             window_idx : SLEEP_WINDOW_SIZE;
            current_sleep_stage = run_sleep_staging(recent_window, count);

            /* Store in history */
            sleep_stage_history[sleep_history_idx] = current_sleep_stage;
            sleep_history_idx = (sleep_history_idx + 1) % SLEEP_HISTORY_SAMPLES;
            if (sleep_history_count < SLEEP_HISTORY_SAMPLES)
                sleep_history_count++;

            /* Update environment based on stage */
            update_environment_setpoints();

            /* Check smart alarm */
            if (should_trigger_alarm())
                trigger_alarm_sequence();
        }

        vTaskDelay(pdMS_TO_TICKS(SLEEP_STAGING_INTERVAL_MS));
    }
}

static void task_audio(void *arg)
{
    audio_init();

    while (1) {
        if (soundscape_id > 0) {
            audio_play_soundscape(soundscape_id, soundscape_volume);
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); /* Update audio every 1s */
    }
}

static void task_display(void *arg)
{
    display_init();
    uint32_t frame = 0;

    while (1) {
        read_hub_sensors();
        display_render();

        /* Compute sleep score every 5 minutes */
        if (frame % 60 == 0) {
            current_sleep_score = compute_sleep_score();
        }

        /* Publish to MQTT every 30s */
        if (frame % 6 == 0) {
            mqtt_publish_sleep_data();
        }

        frame++;
        vTaskDelay(pdMS_TO_TICKS(5000)); /* 5s refresh */
    }
}

static void task_wifi_mqtt(void *arg)
{
    wifi_mqtt_init();

    while (1) {
        /* In production: handle MQTT messages, OTA checks, time sync */
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

/* ---- Button Handlers ---- */

static void gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    if (gpio_num == BTN_SNOOZE) {
        handle_snooze();
    } else if (gpio_num == BTN_MODE) {
        soundscape_id = (soundscape_id + 1) % 8;
    } else if (gpio_num == BTN_BRIGHT) {
        /* Cycle display brightness */
    }
}

/* ---- Main Entry ---- */

void app_main(void)
{
    printf("=== SleepSync Nightstand Hub v1.0 ===\n");
    printf("ESP32-S3-WROOM-1-N8R8\n");

    /* Initialize NVS for WiFi/BLE config */
    nvs_flash_init();

    /* Initialize node state */
    memset(nodes, 0, sizeof(nodes));
    for (int i = 0; i < MAX_MESH_NODES; i++)
        nodes[i].active = false;

    /* Initialize I2C sensors */
    sensors_init();

    /* Configure button GPIOs */
    gpio_config_t btn_config = {
        .pin_bit_mask = (1ULL << BTN_SNOOZE) | (1ULL << BTN_MODE) | (1ULL << BTN_BRIGHT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&btn_config);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BTN_SNOOZE, gpio_isr_handler, (void *)BTN_SNOOZE);
    gpio_isr_handler_add(BTN_MODE, gpio_isr_handler, (void *)BTN_MODE);
    gpio_isr_handler_add(BTN_BRIGHT, gpio_isr_handler, (void *)BTN_BRIGHT);

    /* Create FreeRTOS tasks */
    xTaskCreate(task_ble_mesh, "ble_mesh", 4096, NULL, 5, NULL);
    xTaskCreate(task_sleep_staging, "sleep_staging", 8192, NULL, 4, NULL);
    xTaskCreate(task_audio, "audio", 4096, NULL, 3, NULL);
    xTaskCreate(task_display, "display", 4096, NULL, 2, NULL);
    xTaskCreate(task_wifi_mqtt, "wifi_mqtt", 4096, NULL, 2, NULL);

    printf("Hub initialized. 5 tasks running.\n");
    printf("Waiting for sleep strip to join mesh...\n");
}