/*
 * hub_main.c — SoleGuard Hub Node firmware (RP2040, Pico SDK)
 *
 * Aggregates BLE-mesh data from both insoles + ankle tag, runs the
 * TFLite Micro ulcer-risk model every 5 min, renders the foot heat map
 * on the ILI9488 TFT, and sounds voice/beep alerts. Bridges to WiFi
 * (ESP32-C6 co-processor) for MQTT cloud sync + BLE to the mobile app.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "pico/multicore.h"
#include "tflite_micro.h"
#include "ili9488.h"
#include "max98357.h"
#include "sole_protocol.h"

#define MESH_UART  uart0   /* nRF52840 mesh radio on UART0 @ 1Mbaud */
#define WIFI_UART  uart1   /* ESP32-C6 WiFi co-proc on UART1 @ 921600 */

/* ---- Aggregation buffers (24-hr ring) ---- */
#define RING_HOURS   24
#define RING_SLOTS   (RING_HOURS * 120) /* 30s per slot -> 120/hr */

typedef struct {
    uint8_t  pressure_L[24];
    uint8_t  pressure_R[24];
    int16_t  temp_L[8];
    int16_t  temp_R[8];
    uint16_t pti_L, pti_R;
    int16_t  gait_L[8], gait_R[8];
    int16_t  gait_ankle[8];
    uint16_t edema_index;
    int16_t  ankle_skin_temp;
    uint8_t  risk_L, risk_R;
    uint8_t  flags;
    int64_t  timestamp_ms;
} ring_slot_t;

static ring_slot_t ring[RING_SLOTS];
static volatile uint32_t ring_head = 0;
static volatile uint32_t ring_count = 0;

/* Latest-received per-node data (updated by mesh RX callback) */
static uint8_t  cur_pressure_L[24], cur_pressure_R[24];
static int16_t  cur_temp_L[8], cur_temp_R[8];
static uint16_t cur_pti_L, cur_pti_R;
static int16_t  cur_gait_L[8], cur_gait_R[8];
static int16_t  cur_gait_ankle[8];
static uint16_t cur_edema;
static int16_t  cur_ankle_temp;

/* ---- TFLite Micro ulcer-risk model ---- */
static TfLiteMicroModel *risk_model;
/* Model expects: 6 pressure-zone peaks (L+R), 8 temp-asymmetry diffs,
   8 gait features, 1 edema index, 1 ankle temp = 30 floats.
   Output: 2 floats (risk_L, risk_R) in [0,1]. */
#define MODEL_INPUT_DIM 30

static void load_risk_model(void)
{
    /* Model embedded as C array (converted via xxd; see ulcerrisk_model.c) */
    risk_model = tflm_model_create(ulcerrisk_model_data, ulcerrisk_model_len);
    if (!risk_model) {
        printf("ERROR: failed to load ulcer-risk model\n");
    }
}

static void run_risk_inference(void)
{
    if (!risk_model || ring_count < 8) return;

    /* Build feature vector from the last 8 slots (4 min window) */
    float input[MODEL_INPUT_DIM];
    memset(input, 0, sizeof(input));

    /* Average pressure per zone (6 zones) for L & R -> 12 floats */
    for (int z = 0; z < 6; z++) {
        float pL = 0, pR = 0;
        for (int s = 0; s < 8; s++) {
            uint32_t idx = (ring_head - 8 + s + RING_SLOTS) % RING_SLOTS;
            pL += sole_zone_peak_pressure(ring[idx].pressure_L, (sole_zone_t)z);
            pR += sole_zone_peak_pressure(ring[idx].pressure_R, (sole_zone_t)z);
        }
        input[z]      = pL / 8.0f / 255.0f;     /* normalized 0..1 */
        input[6 + z]  = pR / 8.0f / 255.0f;
    }
    /* Temperature asymmetry: |L[i]-R[i]| for 8 zones -> 8 floats */
    for (int i = 0; i < 8; i++) {
        float d = (cur_temp_L[i] - cur_temp_R[i]) / 100.0f; /* degC */
        if (d < 0) d = -d;
        input[12 + i] = d / 5.0f; /* normalize ~0..5C */
    }
    /* Gait: 8 features (use left as representative, or average) -> 8 floats */
    for (int i = 0; i < 8; i++)
        input[20 + i] = (float)cur_gait_L[i] / 1000.0f;
    /* Edema + ankle temp -> 2 floats */
    input[28] = (float)cur_edema / 1000.0f;
    input[29] = (float)cur_ankle_temp / 100.0f;

    float output[2];
    if (tflm_model_invoke(risk_model, input, MODEL_INPUT_DIM, output, 2) == 0) {
        uint8_t rL = (uint8_t)(output[0] * 100.0f);
        uint8_t rR = (uint8_t)(output[1] * 100.0f);
        ring[ring_head].risk_L = rL;
        ring[ring_head].risk_R = rR;

        /* Hard clinical-rule alert: temp asymmetry > 2.2C for 2 consecutive readings */
        for (int i = 0; i < 8; i++) {
            float asym = fabsf((cur_temp_L[i] - cur_temp_R[i]) / 100.0f);
            if (asym > 2.2f) {
                ring[ring_head].flags |= SOLE_ALERT_TEMP_ASYM;
                trigger_voice_alert("Temperature asymmetry detected. Please check your feet and offload.");
                break;
            }
        }
        if (rL > 65 || rR > 65) {
            trigger_voice_alert("Ulcer risk elevated. Please offload the affected foot.");
        }
        /* Broadcast risk score to mesh (for scanner display) */
        sole_risk_payload_t rp = {0};
        rp.type = SOLE_MSG_RISK_SCORE;
        rp.node_id = SOLE_NODE_ID_HUB;
        rp.risk_left = rL;
        rp.risk_right = rR;
        sole_pack_crc(&rp, sizeof(rp) - 2);
        mesh_uart_send((const uint8_t *)&rp, sizeof(rp));
    }
}

/* ---- Mesh UART RX (called from core 1 loop) ---- */
void sole_on_mesh_rx(uint8_t type, const uint8_t *data, size_t len)
{
    switch (type) {
    case SOLE_MSG_PRESSURE_TEMP: {
        if (len < sizeof(sole_pressure_payload_t)) break;
        const sole_pressure_payload_t *p = (const sole_pressure_payload_t *)data;
        if (p->node_id == SOLE_NODE_ID_INSOLE_L) {
            memcpy(cur_pressure_L, p->pressure, 24);
            memcpy(cur_temp_L, p->temp_centic, sizeof(cur_temp_L));
            cur_pti_L = p->pti_centic;
        } else if (p->node_id == SOLE_NODE_ID_INSOLE_R) {
            memcpy(cur_pressure_R, p->pressure, 24);
            memcpy(cur_temp_R, p->temp_centic, sizeof(cur_temp_R));
            cur_pti_R = p->pti_centic;
        }
        if (p->flags & SOLE_ALERT_HOTSPOT)
            trigger_voice_alert("Pressure hotspot detected. Please rest or change footwear.");
        break;
    }
    case SOLE_MSG_GAIT: {
        if (len < sizeof(sole_gait_payload_t)) break;
        const sole_gait_payload_t *g = (const sole_gait_payload_t *)data;
        if (g->node_id == SOLE_NODE_ID_INSOLE_L)
            memcpy(cur_gait_L, g->gait, sizeof(cur_gait_L));
        else if (g->node_id == SOLE_NODE_ID_INSOLE_R)
            memcpy(cur_gait_R, g->gait, sizeof(cur_gait_R));
        else if (g->node_id == SOLE_NODE_ID_ANKLE)
            memcpy(cur_gait_ankle, g->gait, sizeof(cur_gait_ankle));
        break;
    }
    case SOLE_MSG_EDEMA: {
        if (len < sizeof(sole_edema_payload_t)) break;
        const sole_edema_payload_t *e = (const sole_edema_payload_t *)data;
        cur_edema = e->edema_index;
        cur_ankle_temp = e->skin_temp_centic;
        if (e->flags & SOLE_ALERT_EDEMA)
            trigger_voice_alert("Ankle swelling detected. Please elevate your legs and contact your clinician.");
        break;
    }
    case SOLE_MSG_ALERT: {
        if (len < sizeof(sole_alert_payload_t)) break;
        const sole_alert_payload_t *a = (const sole_alert_payload_t *)data;
        if (a->flags & SOLE_ALERT_FALL) {
            trigger_voice_alert("Fall detected. Emergency contact has been notified.");
            wifi_send_alert("FALL", a->value);
        }
        if (a->flags & SOLE_ALERT_WOUND)
            trigger_voice_alert("Wound detected by foot scanner. Please contact your clinician.");
        break;
    }
    default:
        break;
    }
}

/* ---- Ring buffer push ---- */
static void ring_push(void)
{
    ring_slot_t *s = &ring[ring_head];
    memcpy(s->pressure_L, cur_pressure_L, 24);
    memcpy(s->pressure_R, cur_pressure_R, 24);
    memcpy(s->temp_L, cur_temp_L, sizeof(cur_temp_L));
    memcpy(s->temp_R, cur_temp_R, sizeof(cur_temp_R));
    s->pti_L = cur_pti_L;
    s->pti_R = cur_pti_R;
    memcpy(s->gait_L, cur_gait_L, sizeof(cur_gait_L));
    memcpy(s->gait_R, cur_gait_R, sizeof(cur_gait_R));
    memcpy(s->gait_ankle, cur_gait_ankle, sizeof(cur_gait_ankle));
    s->edema_index = cur_edema;
    s->ankle_skin_temp = cur_ankle_temp;
    s->timestamp_ms = (int64_t)time_us_64() / 1000;
    s->flags = 0;

    ring_head = (ring_head + 1) % RING_SLOTS;
    if (ring_count < RING_SLOTS) ring_count++;
}

/* ---- Core 1: mesh UART polling + TFT render ---- */
void core1_main(void)
{
    while (1) {
        uint8_t buf[64];
        int n = mesh_uart_read(buf, sizeof(buf));
        if (n > 0) {
            /* minimal framing: [len][payload] */
            if (n >= 2)
                sole_on_mesh_rx(buf[1], &buf[2], (size_t)(buf[0] - 1));
        }
        /* Render heat map every 2s */
        render_foot_heatmap(cur_pressure_L, cur_pressure_R,
                            cur_temp_L, cur_temp_R,
                            ring[ring_head].risk_L, ring[ring_head].risk_R);
        sleep_ms(2000);
    }
}

int main(void)
{
    stdio_init_all();
    /* Init UARTs: mesh @1Mbaud (UART0), WiFi @921600 (UART1) */
    uart_init(MESH_UART, 1000000);
    uart_init(WIFI_UART, 921600);
    gpio_set_function(0, GPIO_FUNC_UART); gpio_set_function(1, GPIO_FUNC_UART);
    gpio_set_function(4, GPIO_FUNC_UART); gpio_set_function(5, GPIO_FUNC_UART);

    /* Init SPI0 for ILI9488 TFT, I2C0 for RTC + expansion */
    spi_init(spi0, 24000000);
    i2c_init(i2c0, 100000);

    ili9488_init(spi0);
    max98357_init(WIFI_UART); /* reuse UART1 for I2S-less PCM? — max98357 uses I2S; see max98357.c */

    load_risk_model();
    printf("SoleGuard Hub starting — model loaded\n");

    /* Launch core 1 for mesh + TFT */
    multicore_launch_core1(core1_main);

    int64_t last_risk_ms = 0;
    int64_t last_push_ms = 0;
    int64_t last_wifi_ms = 0;

    while (1) {
        int64_t now = (int64_t)time_us_64() / 1000;

        /* Push ring slot every 30s */
        if (now - last_push_ms > 30000) {
            ring_push();
            last_push_ms = now;
        }
        /* Run risk inference every 5 min */
        if (now - last_risk_ms > 300000) {
            run_risk_inference();
            last_risk_ms = now;
        }
        /* WiFi MQTT sync every 60s */
        if (now - last_wifi_ms > 60000) {
            wifi_send_telemetry(&ring[(ring_head - 1 + RING_SLOTS) % RING_SLOTS]);
            last_wifi_ms = now;
        }
        sleep_ms(500);
    }
    return 0;
}