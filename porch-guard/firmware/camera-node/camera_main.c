/*
 * camera_main.c — PorchGuard Porch Camera Node (ESP32-S3)
 *
 * Responsibilities:
 * - Low-power PIR-gated image capture (camera sleeps until PIR/mmWave fires)
 * - Package detection (on-device TFLite MobileNet-SSD) → DELIVERY_EVENT
 * - Person detection + re-ID embedding vs resident/courier gallery
 * - Pirate behavior telemetry streaming to hub (hub runs temporal model)
 * - Two-way audio (MAX98357A speaker + INMP441 mic) — app-initiated talk
 * - Knock/glass-break detection on mic
 * - Clip recording (5s pre + 5s post event) to MicroSD, WiFi upload to cloud
 * - Sub-GHz mesh client (event channel — works even if WiFi is down)
 * - Tamper detection (tilt sensor) → TAMPER_ALERT
 * - Deterrent chime on unknown stranger loitering >10s
 *
 * Power: USB-C (doorbell transformer 16-24VAC → 5V buck) + 1F supercap
 *        supercap fires final TAMPER_ALERT if power is cut
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/spi.h"
#include "driver/i2s.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "mesh_protocol.h"

/* ---- Pin Definitions (ESP32-S3-WROOM-1) ---- */
#define PIN_SX1261_SPI_HOST  SPI2_HOST
#define PIN_SX1261_SCK       12
#define PIN_SX1261_MOSI      11
#define PIN_SX1261_MISO      13
#define PIN_SX1261_CS        10
#define PIN_SX1261_BUSY      9
#define PIN_SX1261_IRQ       14
#define PIN_SX1261_NRST      46

#define PIN_OV2640_D0        5   /* camera data bus */
#define PIN_OV2640_PCLK      15
#define PIN_OV2640_VSYNC     16
#define PIN_OV2640_HREF      17
#define PIN_OV2640_SIOD      18  /* I2C SCCB */
#define PIN_OV2642_SIOC      19
#define PIN_OV2640_RESET     20
#define PIN_OV2640_PWDN      21
#define PIN_OV2640_XCLK      22

#define PIN_PIR              1   /* AM612 motion wakeup */
#define PIN_MMWAVE_UART_TX   41  /* HLK-LD2410 UART */
#define PIN_MMWAVE_UART_RX   42

#define PIN_MIC_I2S_WS       4   /* INMP441 I2S */
#define PIN_MIC_I2S_SCK       5
#define PIN_MIC_I2S_SD        6
#define PIN_SPK_I2S_WS        7   /* MAX98357A I2S */
#define PIN_SPK_I2S_SCK       8
#define PIN_SPK_I2S_SD        3

#define PIN_WHITE_LED        38  /* illumination */
#define PIN_IR_LED           39  /* 940nm IR array */
#define PIN_TILT_IRQ          40  /* SW-420 tilt sensor */
#define PIN_SD_CS             37
#define PIN_CHG_STAT          36  /* supercap charger status */

/* ---- mmWave presence (HLK-LD2410 parsing) ---- */
typedef struct {
    int16_t  distance_cm;   /* distance to nearest moving target, -1 = none */
    uint16_t energy;        /* motion energy */
    uint8_t  state;         /* 0=none, 1=stationary, 2=moving */
} mmwave_t;
static mmwave_t mmwave = {0, 0, 0};

/* ---- Camera state ---- */
typedef enum {
    CAM_SLEEP = 0,         /* PIR-gated: camera module off, ESP32 light sleep */
    CAM_WAKE = 1,          /* PIR fired, warming up camera */
    CAPTURING = 2,          /* streaming frames for detection */
    RECORDING_CLIP = 3,    /* recording event clip to SD */
    TALK_MODE = 4,          /* two-way audio active */
} cam_state_t;
static cam_state_t cam_state = CAM_SLEEP;

/* ---- Detection state ---- */
typedef struct {
    uint8_t parcel_class;     /* PARCEL_* */
    uint8_t parcel_conf;      /* 0-255 */
    uint8_t person_id;        /* PERSON_* */
    uint8_t person_conf;      /* 0-255 */
    int16_t mmwave_dist_cm;
    uint8_t knock_detected;   /* 0=no,1=knock,2=glass-break */
    uint8_t tamper_flag;      /* 0=ok,1=cover,2=tilt */
    uint8_t clip_ready;
    uint16_t clip_id;
    int16_t ambient_temp_x10;
    uint8_t wifi_up;
    uint8_t power_lost;
    uint8_t battery_pct;      /* supercap charge */
} detection_t;
static detection_t det = {0};

/* ---- Loitering tracker ---- */
static uint32_t stranger_loiter_start_ms = 0;
static bool deterrent_chime_played = false;

/* ---- Clip recording (rolling MicroSD) ---- */
#define CLIP_DURATION_S    10  /* 5s pre + 5s post */
#define MAX_CLIPS          999
static uint16_t next_clip_id = 1;

static void record_clip(uint16_t clip_id, uint8_t event_type)
{
    /* In production: write 10s of OV2640 frames (320x240 MJPEG) to MicroSD
     * as clip_<clip_id>.mjpeg. Pre-buffer kept in PSRAM (5s ring buffer). */
    printf("[CLIP] Recording clip %d (event %d) to SD\n", clip_id, event_type);
    det.clip_ready = 1;
    det.clip_id = clip_id;
}

/* ---- SX1261 Radio (stub) ---- */
static void sx1261_init(void)
{
    /* Configure SPI2 for SX1261, 915MHz, SF7, 125kHz */
    printf("[SX1261] Initialized (915MHz, +14dBm)\n");
}

static void sx1261_send(const uint8_t *data, uint16_t len)
{
    /* TX FIFO write + set TX mode */
    printf("[SX1261] TX %d bytes\n", len);
}

/* ---- Camera + ML (stubs for TFLite Micro) ---- */

static void camera_init(void)
{
    /* Configure OV2640: 320x240 RGB565 @ 10fps, 160° FOV lens
     * Use esp_camera driver. */
    printf("[CAM] OV2640 initialized (320x240 @ 10fps)\n");
}

static void camera_capture_and_detect(void)
{
    /* In production:
     * 1. Capture frame from OV2640 (320x240)
     * 2. Run MobileNet-SSD INT8 for parcel detection → parcel_class + conf + bbox
     * 3. If person present, run person re-ID CNN → 128-d embedding
     * 4. Match embedding against gallery (residents + couriers) in flash
     * 5. Set det.parcel_class, det.parcel_conf, det.person_id, det.person_conf
     *
     * Stub: deterministic demo values to validate mesh path */
    static uint8_t demo_phase = 0;
    demo_phase = (demo_phase + 1) % 8;

    if (demo_phase < 3) {
        det.parcel_class = PARCEL_NONE;
        det.person_id = PERSON_NONE;
        det.mmwave_dist_cm = -1;
    } else if (demo_phase < 5) {
        /* A known resident approaches */
        det.parcel_class = PARCEL_NONE;
        det.person_id = PERSON_RESIDENT;
        det.person_conf = 220;
        det.mmwave_dist_cm = 80;
    } else if (demo_phase == 5) {
        /* Courier drops a parcel */
        det.parcel_class = PARCEL_MEDIUM;
        det.parcel_conf = 200;
        det.person_id = PERSON_COURIER;
        det.person_conf = 180;
        det.mmwave_dist_cm = 60;
        record_clip(next_clip_id++, 0);  /* delivery event */
    } else if (demo_phase == 6) {
        /* Unknown stranger loiters */
        det.parcel_class = PARCEL_MEDIUM;
        det.person_id = PERSON_UNKNOWN;
        det.person_conf = 150;
        det.mmwave_dist_cm = 30;
    } else {
        /* Pirate grabs and flees — parcel gone, stranger still present */
        det.parcel_class = PARCEL_NONE;
        det.person_id = PERSON_LOITERING;
        det.person_conf = 140;
        det.mmwave_dist_cm = 10;
    }
}

/* ---- mmWave presence parsing (HLK-LD2410) ---- */
static void mmwave_poll(void)
{
    /* In production: read UART from HLK-LD2410, parse 0x55 0xAA frame:
     *   - moving target distance + energy
     *   - stationary target distance + energy
     * Sub-meter presence detection distinguishes person vs pet vs package.
     *
     * Stub: rely on det.mmwave_dist_cm set in camera_capture_and_detect */
    if (det.mmwave_dist_cm > 0)
        mmwave.state = 2;
    else
        mmwave.state = 0;
}

/* ---- Mic analysis (knock / glass-break) ---- */
static void mic_poll(void)
{
    /* In production: capture short I2S bursts (50ms) and run an FFT
     * to detect a knock (impulse ~2kHz) or glass-break (broadband 3-8kHz).
     *
     * Stub: deterministic knock detection every ~30s for demo */
    static uint32_t last_knock_check = 0;
    uint32_t now = esp_timer_get_time() / 1000;
    if (now - last_knock_check > 30000) {
        det.knock_detected = 1;
        last_knock_check = now;
        printf("[MIC] Knock detected\n");
    } else {
        det.knock_detected = 0;
    }
}

/* ---- Tilt / tamper ---- */
static void tamper_check(void)
{
    /* In production: read SW-420 tilt IRQ — camera being moved/covered. */
    int tilt = gpio_get_level(PIN_TILT_IRQ);
    if (tilt == 0) {  /* active low */
        det.tamper_flag = 2;
        printf("[TAMPER] Camera tilt detected!\n");
    } else {
        det.tamper_flag = 0;
    }
}

/* ---- Deterrent chime ---- */
static void play_deterrent_chime(void)
{
    /* Play a short escalating tone via MAX98357A to deter loiterer.
     * In production: write I2S samples for a 2-second chime. */
    printf("[AUDIO] Playing deterrent chime (unknown loiterer >10s)\n");
}

/* ---- Build + send camera data packet to hub ---- */
static void send_camera_data(void)
{
    camera_data_payload_t payload = {0};
    payload.presence_state = mmwave.state > 0 ? PRESENCE_PERSON :
                              (det.parcel_class != PARCEL_NONE ? PRESENCE_PARCEL_ONLY : PRESENCE_CLEAR);
    if (mmwave.state > 0 && det.parcel_class != PARCEL_NONE)
        payload.presence_state = PRESENCE_BOTH;
    payload.person_id     = det.person_id;
    payload.person_conf   = det.person_conf;
    payload.parcel_class  = det.parcel_class;
    payload.parcel_conf   = det.parcel_conf;
    payload.pirate_risk   = 0;  /* hub computes this from telemetry stream */
    payload.mmwave_dist_cm = det.mmwave_dist_cm;
    payload.ambient_temp_c_x10 = det.ambient_temp_x10;
    payload.armed         = 1;  /* reflects hub-armed state */
    payload.siren_active  = 0;  /* hub-controlled */
    payload.tamper_flag   = det.tamper_flag;
    payload.knock_detected = det.knock_detected;
    payload.clip_ready    = det.clip_ready;
    payload.clip_id       = det.clip_id;
    payload.battery_pct   = det.battery_pct;
    payload.signal_rssi   = 0;
    payload.wifi_up       = det.wifi_up;
    payload.power_lost    = det.power_lost;

    mesh_packet_t pkt;
    uint16_t len = mesh_build_packet(NODE_ID_CAMERA, NODE_ID_HUB,
                                      PKT_CAMERA_DATA,
                                      (uint8_t *)&payload, sizeof(payload), &pkt);
    sx1261_send((uint8_t *)&pkt, len);
}

static void send_pirate_alert(uint8_t risk_score)
{
    pirate_alert_payload_t pa = {0};
    pa.alert_level = risk_score > 230 ? ALERT_EMERGENCY : ALERT_CRITICAL;
    pa.pirate_risk = risk_score;
    pa.person_id = det.person_id;
    pa.parcel_class = det.parcel_class;
    pa.has_clip = det.clip_ready;
    pa.clip_id = det.clip_id;
    pa.siren_requested = 1;
    pa.source_node = NODE_ID_CAMERA;

    mesh_packet_t pkt;
    uint16_t len = mesh_build_packet(NODE_ID_CAMERA, NODE_ID_BROADCAST,
                                      PKT_PIRATE_ALERT,
                                      (uint8_t *)&pa, sizeof(pa), &pkt);
    /* Pirate alert uses SF12 (max range + robustness) */
    sx1261_send((uint8_t *)&pkt, len);
    printf("[ALERT] Sent PIRATE_ALERT risk=%d\n", risk_score);
}

static void send_delivery_event(uint8_t event_type, uint8_t courier_id)
{
    delivery_event_payload_t dev = {0};
    dev.event_type = event_type;
    dev.parcel_class = det.parcel_class;
    dev.courier_id = courier_id;
    dev.source_node = NODE_ID_CAMERA;
    dev.has_clip = det.clip_ready;
    dev.clip_id = det.clip_id;
    dev.temp_c_x10 = det.ambient_temp_x10;

    mesh_packet_t pkt;
    uint16_t len = mesh_build_packet(NODE_ID_CAMERA, NODE_ID_HUB,
                                      PKT_DELIVERY_EVENT,
                                      (uint8_t *)&dev, sizeof(dev), &pkt);
    sx1261_send((uint8_t *)&pkt, len);
    printf("[EVENT] Sent DELIVERY_EVENT type=%d courier=%d\n", event_type, courier_id);
}

static void send_tamper_alert(uint8_t tamper_type, uint8_t severity)
{
    tamper_alert_payload_t ta = {0};
    ta.alert_level = severity >= 3 ? ALERT_EMERGENCY : ALERT_CRITICAL;
    ta.tamper_type = tamper_type;
    ta.source_node = NODE_ID_CAMERA;
    ta.severity = severity;

    mesh_packet_t pkt;
    uint16_t len = mesh_build_packet(NODE_ID_CAMERA, NODE_ID_BROADCAST,
                                      PKT_TAMPER_ALERT,
                                      (uint8_t *)&ta, sizeof(ta), &pkt);
    sx1261_send((uint8_t *)&pkt, len);
    printf("[ALERT] Sent TAMPER_ALERT type=%d\n", tamper_type);
}

/* ---- Main task ---- */

static void porch_guard_task(void *arg)
{
    printf("=== PorchGuard Camera Node v1.0 ===\n");
    printf("ESP32-S3 + OV2640 + HLK-LD2410\n");

    nvs_flash_init();
    sx1261_init();
    camera_init();

    /* GPIO for PIR wakeup, tilt IRQ, LEDs */
    gpio_set_direction(PIN_PIR, GPIO_MODE_INPUT);
    gpio_set_direction(PIN_TILT_IRQ, GPIO_MODE_INPUT);
    gpio_set_direction(PIN_WHITE_LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_IR_LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_CHG_STAT, GPIO_MODE_INPUT);

    /* Enable PIR as wake source */
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_PIR, 1);

    uint32_t loop_count = 0;
    uint32_t last_pir_wake_ms = 0;

    while (1) {
        uint32_t now_ms = esp_timer_get_time() / 1000;
        int pir = gpio_get_level(PIN_PIR);

        if (pir == 1 && cam_state == CAM_SLEEP) {
            /* PIR fired: wake up camera */
            cam_state = CAM_WAKE;
            last_pir_wake_ms = now_ms;
            printf("[CAM] PIR wake → warming up camera\n");
            /* In production: power on OV2640, allow 300ms warmup */
        }

        if (cam_state >= CAM_WAKE) {
            mmwave_poll();
            camera_capture_and_detect();
            mic_poll();
            tamper_check();

            /* Transition to capturing after warmup */
            if (cam_state == CAM_WAKE && (now_ms - last_pir_wake_ms) > 300)
                cam_state = CAPTURING;
        }

        /* Loitering → deterrent chime after 10s */
        if (det.person_id == PERSON_UNKNOWN || det.person_id == PERSON_LOITERING) {
            if (stranger_loiter_start_ms == 0) {
                stranger_loiter_start_ms = now_ms;
                deterrent_chime_played = false;
            } else if (!deterrent_chime_played &&
                       (now_ms - stranger_loiter_start_ms) > 10000) {
                play_deterrent_chime();
                deterrent_chime_played = true;
            }
        } else {
            stranger_loiter_start_ms = 0;
            deterrent_chime_played = false;
        }

        /* Detect parcel appearance → DELIVERY_EVENT */
        static uint8_t prev_parcel_class = PARCEL_NONE;
        if (det.parcel_class != PARCEL_NONE && prev_parcel_class == PARCEL_NONE &&
            det.person_id == PERSON_COURIER) {
            /* Courier just dropped a parcel */
            send_delivery_event(0, COURIER_AMAZON);  /* demo: Amazon */
            record_clip(next_clip_id++, 0);
        }
        prev_parcel_class = det.parcel_class;

        /* Pirate behavior: parcel gone, stranger present → PIRATE_ALERT */
        if (det.parcel_class == PARCEL_NONE &&
            det.person_id == PERSON_LOITERING) {
            /* Hub runs temporal model; camera sends alert as a hint */
            send_pirate_alert(210);
        }

        /* Tamper */
        if (det.tamper_flag >= 2) {
            send_tamper_alert(1, 3);  /* tilt, emergency */
            det.tamper_flag = 0;       /* one-shot */
        }

        /* Telemetry every 2s when active, every 30s when sleeping */
        bool should_send = (cam_state >= CAPTURING && loop_count % 4 == 0) ||
                           (cam_state == CAM_SLEEP && loop_count % 60 == 0);
        if (should_send) {
            send_camera_data();
        }

        /* Go back to sleep if no presence for 15s */
        if (cam_state >= CAPTURING && mmwave.state == 0 &&
            (now_ms - last_pir_wake_ms) > 15000) {
            cam_state = CAM_SLEEP;
            printf("[CAM] No presence 15s → sleep\n");
            /* In production: power down OV2640, ESP32 light sleep (wake on PIR) */
        }

        /* Status blink */
        vTaskDelay(pdMS_TO_TICKS(500));
        loop_count++;
    }
}

void app_main(void)
{
    xTaskCreate(porch_guard_task, "porch_guard", 8192, NULL, 5, NULL);
}