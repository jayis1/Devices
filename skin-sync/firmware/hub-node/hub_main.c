/*
 * hub_main.c — SkinSync Mirror Hub firmware (RP2040, Pico SDK)
 *
 * Aggregates Sub-GHz mesh data from UV patches + smart dispenser, runs the
 * edge UV-dose calculation (erythema effectiveness × dose → MED fraction),
 * renders the morning skincare dashboard on the ILI9488 TFT, sends
 * dispensing commands to the Smart Dispenser, and bridges to WiFi
 * (ESP32-C6 co-processor) for MQTT cloud sync + BLE to the mobile app.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "pico/multicore.h"
#include "skin_protocol.h"

/* Sub-GHz mesh radio (nRF52832+SX1262 on UART0), WiFi co-proc (ESP32-C6) on UART1 */
#define MESH_UART  uart0
#define WIFI_UART  uart1
#define MESH_BAUD  1000000
#define WIFI_BAUD  921600

/* ---- Per-patch state (supports up to 4 patches) ---- */
#define MAX_PATCHES 4

typedef struct {
    uint8_t  active;
    uint8_t  patch_id;
    uint8_t  fitz_type;
    uint16_t med_jm2;          /* personal MED in J/m² (from cloud) */
    uint16_t uva_total_today;  /* cumulative UVA dose today (J/m² * 10) */
    uint16_t uvb_total_today;
    int16_t  skin_temp_centic;
    uint8_t  uv_index;
    uint8_t  med_fraction;     /* 0-100 */
    uint8_t  battery_pct;
    uint8_t  flags;
    uint8_t  uv_status;
    uint8_t  skin_status;
    uint8_t  skin_cancer_risk;
    uint16_t hours_to_burn;
    int64_t  last_telem_ms;
} patch_state_t;

static patch_state_t patches[MAX_PATCHES];
static uint8_t patch_count = 0;

/* ---- UV dose ring (24-hr at 5-min resolution = 288 slots) ---- */
#define RING_HOURS   24
#define RING_SLOTS   (RING_HOURS * 12)

typedef struct {
    uint8_t  patch_id;
    uint16_t uva_dose;
    uint16_t uvb_dose;
    int16_t  skin_temp;
    uint8_t  uv_index;
    uint8_t  med_fraction;
    uint8_t  uv_status;
    int64_t  timestamp_ms;
} ring_slot_t;

static ring_slot_t ring[RING_SLOTS];
static volatile uint32_t ring_head = 0;
static volatile uint32_t ring_count = 0;

/* ---- Edge UV risk model (stub interface) ---- */
#define MODEL_INPUT_DIM 12
typedef struct { void *handle; } TfLiteMicroModel;

extern const unsigned char *uv_risk_model_data;
extern unsigned int uv_risk_model_len;

TfLiteMicroModel *tflm_model_create(const unsigned char *data, unsigned int len)
{
    (void)data; (void)len;
    return (TfLiteMicroModel *)1;
}

int tflm_model_invoke(TfLiteMicroModel *m, const float *input, int in_dim,
                      float *output, int out_dim)
{
    (void)m; (void)in_dim; (void)out_dim;
    /* Heuristic fallback when ML model not loaded:
     * Compute UV burn risk from MED fraction, UV index, skin temp trend */
    float med_frac = input[0];      /* 0-100 */
    float uv_index = input[1];      /* 0-30 */
    float skin_temp = input[2];     /* degC */
    float skin_temp_baseline = input[3]; /* degC */

    /* Burn risk: MED fraction is primary indicator */
    float burn_risk = med_frac;
    if (uv_index > 8.0f) burn_risk += (uv_index - 8.0f) * 2.0f;
    if (skin_temp - skin_temp_baseline > 2.0f)
        burn_risk += (skin_temp - skin_temp_baseline - 2.0f) * 10.0f;
    if (burn_risk > 100.0f) burn_risk = 100.0f;
    if (burn_risk < 0.0f) burn_risk = 0.0f;

    output[0] = burn_risk / 100.0f;
    output[1] = 0.0f; /* cancer risk (from cloud cumulative model) */
    output[2] = 0.0f; /* skin condition risk (from scanner) */
    return 0;
}

/* ---- Find or create patch state ---- */
static patch_state_t *find_patch(uint8_t patch_id)
{
    for (int i = 0; i < patch_count; i++)
        if (patches[i].active && patches[i].patch_id == patch_id)
            return &patches[i];
    return NULL;
}

static patch_state_t *add_patch(uint8_t patch_id)
{
    if (patch_count >= MAX_PATCHES) return NULL;
    patch_state_t *p = &patches[patch_count++];
    memset(p, 0, sizeof(*p));
    p->active = 1;
    p->patch_id = patch_id;
    p->fitz_type = SS_FITZ_III;  /* default, personalized from cloud */
    p->med_jm2 = 350;             /* default MED for type III */
    p->hours_to_burn = 0xFFFF;
    return p;
}

/* ---- Compute per-patch UV status + risk ---- */
static void compute_patch_status(patch_state_t *p)
{
    const ss_fitz_profile_t *prof = ss_get_fitz_profile(p->fitz_type);
    if (!prof) {
        p->uv_status = SS_UV_SAFE;
        return;
    }

    /* Use personal MED if set, else fitz default */
    uint16_t med = p->med_jm2 > 0 ? p->med_jm2 : prof->med_jm2;

    /* MED fraction: cumulative effective dose / personal MED
     * Telemetry gives dose in J/m² * 10, so divide by 10 */
    float eff_dose = ss_erythema_weighted_dose(
        (float)p->uva_total_today / 10.0f,   /* UVA W/m² proxy */
        (float)p->uvb_total_today / 10.0f,   /* UVB W/m² proxy */
        1.0f                                  /* 1 second integration (simplified) */
    );
    /* In production: integrate actual dose deltas over time, not this proxy.
     * The patch already computes med_fraction on-device; use that. */
    float med_frac = (float)p->med_fraction;

    /* UV status from MED fraction */
    if (med_frac < 50.0f)       p->uv_status = SS_UV_SAFE;
    else if (med_frac < 70.0f)  p->uv_status = SS_UV_CAUTION;
    else if (med_frac < 90.0f)  p->uv_status = SS_UV_WARNING;
    else if (med_frac < 100.0f) p->uv_status = SS_UV_DANGER;
    else                         p->uv_status = SS_UV_BURNED;

    /* Hours to burn at current UV index */
    if (p->uv_index > 0 && med_frac < 100.0f) {
        float uv_idx = (float)p->uv_index / 10.0f;
        /* Rough: MED (J/m²) / (UV index * 25 W/m² * 60) = minutes to burn */
        float remaining_frac = (100.0f - med_frac) / 100.0f;
        float remaining_jm2 = (float)med * remaining_frac;
        float irradiance = uv_idx * 25.0f;  /* rough W/m² per UV index */
        float hours = remaining_jm2 / (irradiance * 3600.0f);
        p->hours_to_burn = (uint16_t)hours;
    } else {
        p->hours_to_burn = 0xFFFF;
    }

    /* Skin temp flush check */
    if (p->flags & SS_ALERT_FLUSH) {
        if (p->uv_status < SS_UV_DANGER)
            p->uv_status = SS_UV_DANGER;
    }

    /* Edge ML risk score */
    float input[MODEL_INPUT_DIM];
    memset(input, 0, sizeof(input));
    input[0] = med_frac;
    input[1] = (float)p->uv_index / 10.0f;
    input[2] = (float)p->skin_temp_centic / 100.0f;
    input[3] = 32.0f;  /* baseline skin temp (would be learned per user) */

    float output[3];
    TfLiteMicroModel *m = tflm_model_create(uv_risk_model_data, uv_risk_model_len);
    tflm_model_invoke(m, input, MODEL_INPUT_DIM, output, 3);
    /* p->skin_cancer_risk set from cloud cumulative model, not edge */
}

/* ---- Mesh RX callback ---- */
static void mesh_rx_handler(uint8_t type, const uint8_t *data, size_t len)
{
    switch (type) {
    case SS_MSG_TELEMETRY: {
        if (len < sizeof(ss_telemetry_payload_t) - 2) break;
        const ss_telemetry_payload_t *p = (const ss_telemetry_payload_t *)data;
        patch_state_t *patch = find_patch(p->node_id);
        if (!patch)
            patch = add_patch(p->node_id);
        if (!patch) break;

        int64_t now = (int64_t)time_us_64() / 1000;

        /* Accumulate dose deltas */
        patch->uva_total_today += p->uva_dose_delta;
        patch->uvb_total_today += p->uvb_dose_delta;
        patch->skin_temp_centic = p->skin_temp_centic;
        patch->uv_index = p->uv_index;
        patch->med_fraction = p->med_fraction;
        patch->battery_pct = p->battery_pct;
        patch->flags = p->flags;
        patch->last_telem_ms = now;

        compute_patch_status(patch);

        /* Store in ring buffer */
        ring[ring_head].patch_id = p->node_id;
        ring[ring_head].uva_dose = patch->uva_total_today;
        ring[ring_head].uvb_dose = patch->uvb_total_today;
        ring[ring_head].skin_temp = p->skin_temp_centic;
        ring[ring_head].uv_index = p->uv_index;
        ring[ring_head].med_fraction = p->med_fraction;
        ring[ring_head].uv_status = patch->uv_status;
        ring[ring_head].timestamp_ms = now;
        ring_head = (ring_head + 1) % RING_SLOTS;
        if (ring_count < RING_SLOTS) ring_count++;

        /* Trigger sun burn alert if needed */
        if (patch->uv_status >= SS_UV_WARNING) {
            printf("UV ALERT: patch %u med=%u%% status=%s\n",
                   patch->patch_id, patch->med_fraction,
                   ss_uv_status_name(patch->uv_status));
            /* In production: push BLE notification to mobile app */
        }
        break;
    }
    case SS_MSG_DISPENSE_ACK: {
        if (len < sizeof(ss_dispense_ack_payload_t) - 2) break;
        const ss_dispense_ack_payload_t *p = (const ss_dispense_ack_payload_t *)data;
        printf("Dispense ack: slot=%u status=%u mg=%u remaining=%u\n",
               p->slot, p->status, p->mg_dispensed, p->mg_remaining);
        break;
    }
    case SS_MSG_SCAN_RESULT: {
        if (len < sizeof(ss_scan_result_payload_t) - 2) break;
        const ss_scan_result_payload_t *p = (const ss_scan_result_payload_t *)data;
        printf("Scan: loc=%u condition=%s conf=%u abcde=%u skin_age=%u lesion=%u\n",
               p->body_location, ss_condition_name(p->condition_class),
               p->condition_conf, p->abcde_score, p->skin_age, p->lesion_id);

        /* Update skin status based on scan */
        if (p->flags & SS_ALERT_LESION) {
            /* Lesion change detected — high priority */
            printf("⚠ LESION CHANGE: lesion %u abcde=%u — see dermatologist\n",
                   p->lesion_id, p->abcde_score);
        }
        if (p->flags & SS_ALERT_CONDITION) {
            printf("Condition: %s (conf %u%%)\n",
                   ss_condition_name(p->condition_class), p->condition_conf);
        }
        break;
    }
    default:
        break;
    }
}

/* ---- UART transport for mesh (nRF52832+SX1262 on UART0) ---- */
static int mesh_uart_tx(const uint8_t *data, size_t len)
{
    uart_write_blocking(MESH_UART, data, len);
    return (int)len;
}

/* ---- WiFi co-processor bridge (ESP32-C6 on UART1) ---- */
static void wifi_send_to_cloud(const char *json)
{
    uart_write_blocking(WIFI_UART, (const uint8_t *)json, strlen(json));
    uart_write_blocking(WIFI_UART, (const uint8_t *)"\n", 1);
}

/* ---- Morning routine: send dispensing commands ---- */
static void morning_routine(void)
{
    /* Dispense morning skincare:
     * Slot 0: cleanser (if morning wash) — 1.5ml = 1500mg
     * Slot 1: serum (vitamin C) — 0.5ml = 500mg
     * Slot 2: moisturizer — 0.8ml = 800mg
     * Slot 3: sunscreen — 1.2ml = 1200mg (face + neck)
     *
     * Amounts are personalized by cloud ML based on face area, skin type,
     * and condition. These are defaults. */
    printf("=== Morning Routine ===\n");
    ss_send_dispense_cmd(1, 500, 0);   /* serum */
    sleep_ms(2000);                     /* wait for dispense */
    ss_send_dispense_cmd(2, 800, 0);   /* moisturizer */
    sleep_ms(2000);
    ss_send_dispense_cmd(3, 1200, 0);  /* sunscreen */
    printf("Routine dispensed: serum + moisturizer + sunscreen\n");
}

/* ---- Main ---- */
int main(void)
{
    stdio_init_all();

    /* Initialize UARTs */
    uart_init(MESH_UART, MESH_BAUD);
    uart_init(WIFI_UART, WIFI_BAUD);
    gpio_set_function(0, GPIO_FUNC_UART);  /* TX0 */
    gpio_set_function(1, GPIO_FUNC_UART);  /* RX0 */
    gpio_set_function(4, GPIO_FUNC_UART);  /* TX1 */
    gpio_set_function(5, GPIO_FUNC_UART);  /* RX1 */

    /* Initialize mesh */
    ss_mesh_set_tx(mesh_uart_tx);
    ss_mesh_set_rx_callback(mesh_rx_handler);

    /* Initialize SPI for TFT + SD + Flash */
    spi_init(spi0, 24000000);

    /* Initialize I2C for RTC + BME280 */
    i2c_init(i2c0, 100000);

    uint32_t last_inference_ms = 0;
    uint32_t last_cloud_sync_ms = 0;
    uint32_t last_routine_check_ms = 0;
    uint8_t routine_done_today = 0;

    while (1) {
        /* Poll mesh UART for incoming frames */
        if (uart_is_readable(MESH_UART)) {
            uint8_t buf[64];
            int n = 0;
            while (uart_is_readable(MESH_UART) && n < 64) {
                buf[n++] = uart_getc(MESH_UART);
            }
            ss_mesh_on_rx(buf, n);
        }

        uint32_t now = (uint32_t)(time_us_64() / 1000);

        /* Morning routine check (once per day at ~7am, simplified) */
        if (now - last_routine_check_ms > 60000 && !routine_done_today) {
            last_routine_check_ms = now;
            /* In production: check RTC for 7am */
            morning_routine();
            routine_done_today = 1;
        }

        /* Edge inference + risk broadcast every 5 min (300000 ms) */
        if (now - last_inference_ms > 300000) {
            last_inference_ms = now;

            for (int i = 0; i < patch_count; i++) {
                if (!patches[i].active) continue;

                /* Broadcast risk score to mesh (patch uses it for haptic alerts) */
                ss_send_risk_score(patches[i].patch_id,
                                   patches[i].uv_status,
                                   patches[i].med_fraction,
                                   patches[i].skin_cancer_risk,
                                   patches[i].skin_status,
                                   patches[i].hours_to_burn);
            }
        }

        /* Cloud sync every 15 min (900000 ms) */
        if (now - last_cloud_sync_ms > 900000) {
            last_cloud_sync_ms = now;

            for (int i = 0; i < patch_count; i++) {
                if (!patches[i].active) continue;
                char json[256];
                snprintf(json, sizeof(json),
                    "{\"type\":\"uv\",\"patch\":%u,\"uva\":%u,\"uvb\":%u,"
                    "\"temp_c\":%.2f,\"uv_idx\":%.1f,\"med_frac\":%u,\"batt\":%u,"
                    "\"uv_status\":%u,\"hours_to_burn\":%u}",
                    patches[i].patch_id, patches[i].uva_total_today,
                    patches[i].uvb_total_today,
                    patches[i].skin_temp_centic / 100.0f,
                    patches[i].uv_index / 10.0f,
                    patches[i].med_fraction, patches[i].battery_pct,
                    patches[i].uv_status, patches[i].hours_to_burn);
                wifi_send_to_cloud(json);
            }
        }

        sleep_ms(10);
    }

    return 0;
}