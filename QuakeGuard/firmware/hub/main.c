/*
 * quakeguard_hub.c — QuakeGuard Hub firmware (ESP32-S3)
 *
 * Central coordinator:
 *   - Receives SEISMIC_CANDIDATE from Floor Nodes via Sub-GHz
 *   - Runs P-wave/S-wave CNN classifier (tflite-micro, 18 KB model)
 *   - Multi-node consensus (2+ nodes within 500 ms)
 *   - Dispatches SHUTOFF_NOW to Auto-Shutoff Controller
 *   - Polls Structural Tags post-event for damage assessment
 *   - Dispatches family safety check-in via Wi-Fi or 4G LTE
 *   - E-ink display, siren, LED ring, haptic feedback
 *
 * License: MIT
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/uart.h"
#include "driver/spi_master.h"
#include "sdkconfig.h"

#include "common/quakeguard_protocol.h"
#include "common/cc1101.h"

static const char *TAG = "QG_HUB";

/* ── Pin Definitions (ESP32-S3) ─────────────────────────────── */
#define PIN_I2S_BCK    GPIO_NUM_4
#define PIN_I2S_LRCK   GPIO_NUM_5
#define PIN_I2S_DIN    GPIO_NUM_6
#define PIN_PIR         GPIO_NUM_7
#define PIN_I2C_SDA    GPIO_NUM_8
#define PIN_I2C_SCL    GPIO_NUM_9
#define PIN_SPI_CLK    GPIO_NUM_10
#define PIN_SPI_MISO   GPIO_NUM_11
#define PIN_SPI_MOSI   GPIO_NUM_12
#define PIN_CC1101_CS  GPIO_NUM_13
#define PIN_CC1101_GD0 GPIO_NUM_14
#define PIN_EINK_CS    GPIO_NUM_15
#define PIN_EINK_DC    GPIO_NUM_16
#define PIN_UART2_TX   GPIO_NUM_17  /* SIM7000 RXD */
#define PIN_UART2_RX   GPIO_NUM_18  /* SIM7000 TXD */
#define PIN_SIM_PWRKEY GPIO_NUM_21
#define PIN_SD_CS       GPIO_NUM_35
#define PIN_EINK_RST   GPIO_NUM_36
#define PIN_EINK_BUSY   GPIO_NUM_37
#define PIN_LED_RING    GPIO_NUM_48

/* ── Constants ──────────────────────────────────────────────── */
#define CONSENSUS_WINDOW_MS    500   /* 2+ nodes within 500 ms   */
#define S_WAVE_WATCH_MS        10000  /* wait up to 10 s for S-wave */
#define MAX_FLOOR_NODES        8
#define MAX_STRUCT_TAGS        6
#define SEISMIC_BUF_LEN       2048  /* per-node waveform samples  */

/* ── Global State ───────────────────────────────────────────── */
typedef struct {
    uint8_t addr;
    uint8_t active;
    int64_t last_heartbeat_us;
    int16_t waveform[SEISMIC_BUF_LEN * 3]; /* X,Y,Z packed */
    uint16_t sample_count;
    uint8_t  candidate_pending;
    int64_t  candidate_time_us;
} floor_node_t;

typedef struct {
    uint8_t addr;
    uint8_t active;
    int64_t last_report_us;
    int32_t strain_max;
    int32_t strain_mean;
    int16_t resonance_shift;
    uint8_t anomaly_score;
} struct_tag_t;

typedef enum {
    STATE_NORMAL,
    STATE_P_WAVE_DETECTED,
    STATE_S_WAVE_WATCH,
    STATE_EVENT_ACTIVE,
    STATE_POST_EVENT,
} hub_state_t;

static cc1101_t radio;
static floor_node_t floor_nodes[MAX_FLOOR_NODES];
static struct_tag_t struct_tags[MAX_STRUCT_TAGS];
static hub_state_t hub_state = STATE_NORMAL;
static uint8_t seq_num = 0;
static QueueHandle_t rx_queue;

/* ── tflite-micro CNN (P-wave/S-wave classifier) ────────────── */
/* Model: 1D CNN, 5 conv + 2 FC, 18 KB, 3-class output
 * 0 = noise, 1 = P-wave, 2 = S-wave
 * Trained on STEAD dataset + synthetic household noise
 */
extern const unsigned char p_wave_cnn_tflite[];
extern const unsigned int p_wave_cnn_tflite_len;

/* Forward declarations */
static int classify_waveform(const int16_t *waveform, int len,
                               uint8_t *out_class, float *out_confidence);
static void consensus_check(void);
static void dispatch_shutoff(uint8_t action_flags, uint8_t urgency);
static void poll_structural_tags(uint16_t event_id);
static void family_checkin_dispatch(uint16_t event_id);
static void update_display(void);
static void siren_alert(uint8_t severity);
static void led_ring_set(uint8_t color);  /* 0=green, 1=yellow, 2=red */

/* ── Sub-GHz RX Task ────────────────────────────────────────── */
static void rx_task(void *arg)
{
    uint8_t rx_buf[128];
    uint8_t rx_len;
    int8_t rssi;

    while (1) {
        if (cc1101_recv(&radio, rx_buf, &rx_len, &rssi) == 0) {
            /* Parse frame */
            qg_frame_t frame;
            if (qg_parse_frame(rx_buf, rx_len, &frame) != 0)
                continue;

            /* Dispatch by message type */
            switch (frame.header.msg_type) {
            case MSG_HEARTBEAT: {
                /* Update node status */
                for (int i = 0; i < MAX_FLOOR_NODES; i++) {
                    if (floor_nodes[i].addr == frame.header.src_addr) {
                        floor_nodes[i].last_heartbeat_us = esp_timer_get_time();
                        floor_nodes[i].active = 1;
                        break;
                    }
                    if (!floor_nodes[i].active) {
                        floor_nodes[i].addr = frame.header.src_addr;
                        floor_nodes[i].active = 1;
                        floor_nodes[i].last_heartbeat_us = esp_timer_get_time();
                        break;
                    }
                }
                break;
            }
            case MSG_SEISMIC_CANDIDATE: {
                /* Floor node detected acceleration anomaly */
                ESP_LOGW(TAG, "SEISMIC_CANDIDATE from 0x%02X", frame.header.src_addr);

                /* Find or register the node */
                floor_node_t *fn = NULL;
                for (int i = 0; i < MAX_FLOOR_NODES; i++) {
                    if (floor_nodes[i].addr == frame.header.src_addr) {
                        fn = &floor_nodes[i];
                        break;
                    }
                }
                if (!fn) {
                    for (int i = 0; i < MAX_FLOOR_NODES; i++) {
                        if (!floor_nodes[i].active) {
                            floor_nodes[i].addr = frame.header.src_addr;
                            floor_nodes[i].active = 1;
                            fn = &floor_nodes[i];
                            break;
                        }
                    }
                }
                if (!fn) break;

                fn->candidate_pending = 1;
                fn->candidate_time_us = esp_timer_get_time();

                /* If this is the first candidate, start consensus window */
                if (hub_state == STATE_NORMAL) {
                    hub_state = STATE_P_WAVE_DETECTED;
                    ESP_LOGW(TAG, "First candidate — starting consensus window");
                    /* Run CNN on the waveform chunk */
                    seismic_payload_t *sp = (seismic_payload_t *)frame.payload;
                    /* Decompress waveform (simplified: copy raw) */
                    /* In production: delta-decode + RLE decompress */
                    uint8_t wave_class;
                    float confidence;
                    int result = classify_waveform(
                        (int16_t *)sp->data,
                        sp->total_chunks == 1 ? 60 : 30,
                        &wave_class, &confidence
                    );

                    if (result == 0 && wave_class == 1) {
                        /* P-wave classified! */
                        ESP_LOGW(TAG, "P-wave classified (conf=%.2f)", confidence);
                        siren_alert(SEV_MINOR);
                        led_ring_set(1);  /* yellow */
                        hub_state = STATE_S_WAVE_WATCH;

                        /* Start S-wave watch timer */
                        /* (implemented via FreeRTOS timer in production) */
                    } else if (result == 0 && wave_class == 2) {
                        /* S-wave classified directly (very close epicenter) */
                        ESP_LOGW(TAG, "S-wave classified (conf=%.2f)", confidence);
                        consensus_check();
                    }
                } else if (hub_state == STATE_P_WAVE_DETECTED ||
                           hub_state == STATE_S_WAVE_WATCH) {
                    /* Additional node confirms */
                    ESP_LOGW(TAG, "Additional candidate from 0x%02X", frame.header.src_addr);
                    consensus_check();
                }
                break;
            }
            case MSG_SHUTOFF_ACK: {
                shutoff_ack_payload_t *ack = (shutoff_ack_payload_t *)frame.payload;
                ESP_LOGI(TAG, "Shutoff ACK: gas=%d water=%d H2=%dppm CH4=%dppm",
                         ack->gas_valve_closed, ack->water_valve_closed,
                         ack->h2_ppm, ack->ch4_ppm);
                /* Check for gas leak */
                if (ack->h2_ppm > 100 || ack->ch4_ppm > 100) {
                    ESP_LOGE(TAG, "GAS LEAK DETECTED post-shutoff!");
                    siren_alert(SEV_SEVERE);
                    led_ring_set(2);  /* red */
                }
                break;
            }
            case MSG_STRUCT_REPORT: {
                struct_report_payload_t *sr = (struct_report_payload_t *)frame.payload;
                for (int i = 0; i < MAX_STRUCT_TAGS; i++) {
                    if (struct_tags[i].addr == frame.header.src_addr || !struct_tags[i].active) {
                        struct_tags[i].addr = frame.header.src_addr;
                        struct_tags[i].active = 1;
                        struct_tags[i].strain_max = sr->strain_max_micro;
                        struct_tags[i].strain_mean = sr->strain_mean_micro;
                        struct_tags[i].resonance_shift = sr->resonance_shift_hz;
                        struct_tags[i].anomaly_score = sr->anomaly_score;
                        struct_tags[i].last_report_us = esp_timer_get_time();
                        break;
                    }
                }
                ESP_LOGI(TAG, "Struct report from 0x%02X: strain_max=%d με, resonance Δ=%d Hz, anomaly=%d",
                         frame.header.src_addr, sr->strain_max_micro,
                         sr->resonance_shift_hz, sr->anomaly_score);
                break;
            }
            default:
                ESP_LOGD(TAG, "Unknown msg type 0x%02X", frame.header.msg_type);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ── P-Wave/S-Wave CNN Inference ────────────────────────────── */
static int classify_waveform(const int16_t *waveform, int len,
                              uint8_t *out_class, float *out_confidence)
{
    /* tflite-micro inference
     * Input: 3-axis acceleration, 2000 samples × 3 (int16)
     * Model: p_wave_cnn.tflite (18 KB)
     * Output: 3-class softmax (noise / P-wave / S-wave)
     *
     * Full tflite-micro setup omitted for brevity — see
     * software/ml-pipeline/convert_models.py for conversion.
     * The model is compiled into the firmware as a C array
     * (p_wave_cnn_tflite[]).
     *
     * In production:
     *   tflite::MicroMutableOpResolver<10> resolver;
     *   resolver.AddReshape();
     *   resolver.AddConv1D();
     *   resolver.AddMaxPool1D();
     *   resolver.AddFullyConnected();
     *   resolver.AddSoftmax();
     *   tflite::MicroInterpreter interpreter(model, resolver, tensor_arena, 40*1024);
     *   interpreter.Invoke();
     */

    /* Placeholder: simple threshold-based detection
     * In production, replace with tflite-micro inference
     */
    float peak = 0;
    for (int i = 0; i < len * 3; i++) {
        float v = abs((int)waveform[i]) / 32768.0f;
        if (v > peak) peak = v;
    }

    *out_confidence = peak;

    if (peak > 0.8f) {
        *out_class = 2;  /* S-wave (high amplitude) */
        return 0;
    } else if (peak > 0.3f) {
        *out_class = 1;  /* P-wave (moderate amplitude) */
        return 0;
    } else {
        *out_class = 0;  /* noise */
        return 0;
    }
}

/* ── Consensus Check ────────────────────────────────────────── */
static void consensus_check(void)
{
    int64_t now = esp_timer_get_time();
    int confirmations = 0;

    for (int i = 0; i < MAX_FLOOR_NODES; i++) {
        if (floor_nodes[i].active &&
            floor_nodes[i].candidate_pending &&
            (now - floor_nodes[i].candidate_time_us) <
             (CONSENSUS_WINDOW_MS * 1000)) {
            confirmations++;
        }
    }

    if (confirmations >= 2) {
        ESP_LOGE(TAG, "SEISMIC EVENT CONFIRMED (%d nodes)", confirmations);

        /* Estimate magnitude from peak acceleration */
        uint8_t magnitude_x10 = 50;  /* placeholder: M5.0 */

        /* Broadcast SEISMIC_CONFIRMED to all nodes */
        seismic_confirmed_payload_t scp = {
            .timestamp_utc = (uint32_t)(now / 1000000),
            .severity = SEV_MAJOR,
            .magnitude_x10 = magnitude_x10,
            .epicenter_dist_km = 50,
            .actions_taken = QG_ACT_ALL,
            .node_count = confirmations,
        };

        qg_frame_t frame;
        size_t frame_len = qg_build_frame(&frame,
            MSG_SEISMIC_CONFIRMED, QG_ADDR_HUB, QG_ADDR_BROADCAST,
            seq_num++, (uint8_t *)&scp, sizeof(scp));
        cc1101_send(&radio, (uint8_t *)&frame + QG_PREAMBLE_LEN,
                    frame_len - QG_PREAMBLE_LEN);

        /* Dispatch shutoff */
        dispatch_shutoff(QG_ACT_ALL, 2);  /* immediate */

        /* Full alert */
        siren_alert(SEV_MAJOR);
        led_ring_set(2);  /* red */

        /* Family check-in */
        family_checkin_dispatch((uint16_t)(now / 1000));

        /* Poll structural tags (delayed — after shaking stops) */
        /* In production: FreeRTOS timer for 30 s delay */
        poll_structural_tags((uint16_t)(now / 1000));

        hub_state = STATE_EVENT_ACTIVE;

        /* Clear candidates */
        for (int i = 0; i < MAX_FLOOR_NODES; i++)
            floor_nodes[i].candidate_pending = 0;
    }
}

/* ── Dispatch Shutoff ───────────────────────────────────────── */
static void dispatch_shutoff(uint8_t action_flags, uint8_t urgency)
{
    ESP_LOGW(TAG, "Dispatching SHUTOFF_NOW: flags=0x%02X urgency=%d",
             action_flags, urgency);

    shutoff_now_payload_t sp = {
        .action_flags = action_flags,
        .urgency = urgency,
        .event_id = (uint16_t)(esp_timer_get_time() / 1000),
    };

    qg_frame_t frame;
    size_t frame_len = qg_build_frame(&frame,
        MSG_SHUTOFF_NOW, QG_ADDR_HUB, QG_ADDR_SHUTOFF,
        seq_num++, (uint8_t *)&sp, sizeof(sp));

    /* Send with retry (ACK expected within 1 s) */
    for (int retry = 0; retry < 3; retry++) {
        cc1101_send(&radio, (uint8_t *)&frame + QG_PREAMBLE_LEN,
                    frame_len - QG_PREAMBLE_LEN);
        /* Wait for ACK (rx_task will process MSG_SHUTOFF_ACK) */
        vTaskDelay(pdMS_TO_TICKS(1000));
        /* In production: check a flag set by rx_task */
        break;
    }
}

/* ── Poll Structural Tags ───────────────────────────────────── */
static void poll_structural_tags(uint16_t event_id)
{
    struct_poll_payload_t pp = { .event_id = event_id };

    qg_frame_t frame;
    size_t frame_len = qg_build_frame(&frame,
        MSG_STRUCT_POLL, QG_ADDR_HUB, QG_ADDR_BROADCAST,
        seq_num++, (uint8_t *)&pp, sizeof(pp));

    cc1101_send(&radio, (uint8_t *)&frame + QG_PREAMBLE_LEN,
                frame_len - QG_PREAMBLE_LEN);

    ESP_LOGI(TAG, "Polled structural tags for event %d", event_id);
}

/* ── Family Check-in Dispatch ──────────────────────────────── */
static void family_checkin_dispatch(uint16_t event_id)
{
    ESP_LOGW(TAG, "Dispatching family check-in for event %d", event_id);

    /* In production: publish to MQTT cloud (via Wi-Fi or SIM7000 LTE)
     *
     * MQTT topic: quakeguard/{hub_id}/event
     * Payload: {"event_id": 12345, "timestamp": "...",
     *           "magnitude": 5.2, "actions": "gas+water shutoff",
     *           "question": "Are you safe?"}
     *
     * Cloud sends Firebase push to all family members.
     * Family responses (safe/help/noreply) are aggregated and
     * displayed on Hub e-ink + forwarded to emergency contacts.
     */

    /* For now: send via SIM7000 if Wi-Fi unavailable */
    /* SIM7000 AT commands:
     *   AT+CGATT=1            (attach to network)
     *   AT+CIPSTART="TCP","api.quakeguard.io",8883
     *   AT+CIPSEND=<len>
     *   <MQTT connect packet>
     */
}

/* ── Siren Alert ────────────────────────────────────────────── */
static void siren_alert(uint8_t severity)
{
    /* I2S output to MAX98357A
     * Severity determines pattern:
     *   SEV_MINOR:    single beep + haptic
     *   SEV_MODERATE: double beep + haptic
     *   SEV_MAJOR:    continuous siren (105 dB)
     *   SEV_SEVERE:   continuous siren + voice "EARTHQUAKE. EVACUATE."
     */
    ESP_LOGW(TAG, "Siren alert: severity=%d", severity);

    /* In production: generate I2S waveform or play pre-recorded PCM */
    /* Haptic via DRV2605L I2C trigger */
}

/* ── LED Ring ───────────────────────────────────────────────── */
static void led_ring_set(uint8_t color)
{
    /* SK6812 24-LED ring via RMT peripheral
     * 0 = green (normal), 1 = yellow (pre-alert), 2 = red (alert)
     */
    (void)color;  /* RMT setup omitted for brevity */
}

/* ── Display Update ─────────────────────────────────────────── */
static void update_display(void)
{
    /* 2.9" e-ink: show system status, last event, node count
     * In production: SPI commands to Waveshare epd2in9 V2
     */
}

/* ── Main ───────────────────────────────────────────────────── */
void app_main(void)
{
    ESP_LOGI(TAG, "QuakeGuard Hub starting...");

    /* Initialize SPI bus (shared by CC1101 + e-ink + SD card) */
    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_SPI_MISO,
        .mosi_io_num = PIN_SPI_MOSI,
        .sclk_io_num = PIN_SPI_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    /* Initialize I2C (DS3231, DRV2605L, SHT40) */
    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    i2c_param_config(I2C_NUM_0, &i2c_cfg);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

    /* Initialize CC1101 Sub-GHz radio */
    cc1101_init(&radio, SPI2_HOST, PIN_CC1101_CS,
                PIN_CC1101_GD0, -1, QG_ADDR_HUB);

    /* Initialize SIM7000 LTE modem (UART2) */
    uart_config_t uart_cfg = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_param_config(UART_NUM_2, &uart_cfg);
    uart_set_pin(UART_NUM_2, PIN_UART2_TX, PIN_UART2_RX,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_2, 1024, 1024, 0, NULL, 0);

    /* SIM7000 power-on: pulse PWRKEY low for 1 s */
    gpio_set_direction(PIN_SIM_PWRKEY, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_SIM_PWRKEY, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));
    gpio_set_level(PIN_SIM_PWRKEY, 1);
    vTaskDelay(pdMS_TO_TICKS(5000));  /* wait for network registration */

    /* Initialize LED ring, e-ink, haptic */
    led_ring_set(0);  /* green = normal */

    /* Initialize floor node tracking */
    memset(floor_nodes, 0, sizeof(floor_nodes));
    memset(struct_tags, 0, sizeof(struct_tags));

    /* Create RX queue */
    rx_queue = xQueueCreate(32, sizeof(qg_frame_t));

    /* Start Sub-GHz RX task (highest priority) */
    xTaskCreatePinnedToCore(rx_task, "rx_task", 8192, NULL,
                            15, NULL, 1);

    /* Main loop: state machine + heartbeat + display */
    while (1) {
        /* Heartbeat: broadcast every 60 s */
        heartbeat_payload_t hb = {
            .battery_pct = 100,
            .temperature_c = 250,
            .status_flags = 0x01,
            .uptime_hours = 0,
            .rssi_db = 0xFFFF,
        };

        /* Check for timed-out nodes */
        int64_t now = esp_timer_get_time();
        for (int i = 0; i < MAX_FLOOR_NODES; i++) {
            if (floor_nodes[i].active &&
                (now - floor_nodes[i].last_heartbeat_us) > 120000000) {
                ESP_LOGW(TAG, "Floor node 0x%02X timed out",
                         floor_nodes[i].addr);
                floor_nodes[i].active = 0;
            }
        }

        /* State machine transitions */
        if (hub_state == STATE_S_WAVE_WATCH) {
            /* Check if S-wave watch timer expired without S-wave */
            /* If 10 s elapsed → downgrade to minor event */
            /* (Implemented with FreeRTOS timers in production) */
        }
        if (hub_state == STATE_EVENT_ACTIVE) {
            /* Post-event: wait 30 s, then poll structural tags */
            /* Then transition to STATE_POST_EVENT */
            /* After structural assessment → STATE_NORMAL */
        }

        update_display();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}