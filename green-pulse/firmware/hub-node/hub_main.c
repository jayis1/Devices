/*
 * hub_main.c — GreenPulse Hub Node firmware (RP2040, Pico SDK)
 *
 * Aggregates Sub-GHz mesh data from plant tags + water valve, runs the
 * edge disease-risk + watering-prediction heuristics, renders the plant
 * grid on the ILI9488 TFT, triggers watering commands to the valve node,
 * and bridges to WiFi (ESP32-C6 co-processor) for MQTT cloud sync + BLE
 * to the mobile app.
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
#include "green_protocol.h"

/* Sub-GHz mesh radio (nRF52832+SX1262 on UART0), WiFi co-proc (ESP32-C6) on UART1 */
#define MESH_UART  uart0
#define WIFI_UART  uart1
#define MESH_BAUD  1000000
#define WIFI_BAUD  921600

/* ---- Per-plant state (supports up to 32 tags) ---- */
#define MAX_PLANTS 32

typedef struct {
    uint8_t  active;
    uint8_t  tag_id;
    uint8_t  profile_id;
    uint8_t  soil_moisture;
    uint16_t ambient_lux;
    int16_t  temp_centic;
    uint16_t humidity_centi;
    uint8_t  battery_pct;
    uint8_t  flags;
    uint8_t  status;
    uint8_t  disease_risk;
    uint8_t  water_risk;
    uint8_t  light_risk;
    uint16_t hours_to_water;   /* 0xFFFF = not needed */
    int64_t  last_telem_ms;
    int64_t  last_watered_ms;
    /* Drying-curve learning (EWMA of moisture decline rate) */
    float    dry_rate_pct_per_hr;
} plant_state_t;

static plant_state_t plants[MAX_PLANTS];
static uint8_t plant_count = 0;

/* ---- Aggregation ring (24-hr at 15-min resolution = 96 slots) ---- */
#define RING_HOURS   24
#define RING_SLOTS   (RING_HOURS * 4)

typedef struct {
    uint8_t  tag_id;
    uint8_t  soil_moisture;
    uint16_t ambient_lux;
    int16_t  temp_centic;
    uint16_t humidity_centi;
    uint8_t  battery_pct;
    uint8_t  disease_risk;
    uint8_t  water_risk;
    uint8_t  status;
    int64_t  timestamp_ms;
} ring_slot_t;

static ring_slot_t ring[RING_SLOTS];
static volatile uint32_t ring_head = 0;
static volatile uint32_t ring_count = 0;

/* ---- Edge ML disease-risk model (stub interface) ---- */
#define MODEL_INPUT_DIM 16
typedef struct { void *handle; } TfLiteMicroModel;

extern const unsigned char *disease_model_data;
extern unsigned int disease_model_len;

TfLiteMicroModel *tflm_model_create(const unsigned char *data, unsigned int len)
{
    (void)data; (void)len;
    return (TfLiteMicroModel *)1;  /* stub */
}

int tflm_model_invoke(TfLiteMicroModel *m, const float *input, int in_dim,
                      float *output, int out_dim)
{
    (void)m; (void)in_dim; (void)out_dim;
    /* Heuristic fallback when ML model not loaded:
     * Compute disease risk from moisture, humidity, temp, light history */
    float moisture = input[0];     /* % */
    float humidity = input[1];      /* % */
    float temp = input[2];          /* C */
    float light_var = input[3];     /* lux variance (low = stagnant) */
    float moisture_std = input[4]; /* high std = erratic watering */

    /* Powdery mildew: high humidity + moderate temp + stagnant air */
    float mildew_risk = 0;
    if (humidity > 70.0f) mildew_risk += (humidity - 70.0f) * 1.5f;
    if (temp > 20.0f && temp < 28.0f) mildew_risk += 10.0f;
    if (light_var < 50.0f) mildew_risk += 5.0f;  /* stagnant air */

    /* Root rot: consistently wet soil */
    float rot_risk = 0;
    if (moisture > 80.0f) rot_risk += (moisture - 80.0f) * 2.0f;
    rot_risk += moisture_std * 0.3f;  /* erratic watering compounds rot */

    /* Fungal general: high humidity + temp */
    float fungal = (humidity > 60.0f ? (humidity - 60.0f) * 0.8f : 0) +
                   (temp > 22.0f ? (temp - 22.0f) * 1.0f : 0);

    float disease = mildew_risk;
    if (rot_risk > disease) disease = rot_risk;
    if (fungal > disease) disease = fungal;
    if (disease > 100.0f) disease = 100.0f;
    if (disease < 0.0f) disease = 0.0f;

    output[0] = disease / 100.0f;
    output[1] = 0.0f;  /* pest risk (from scanner, not telemetry) */
    output[2] = 0.0f;  /* nutrient risk */
    return 0;
}

/* ---- Find or create plant state ---- */
static plant_state_t *find_plant(uint8_t tag_id)
{
    for (int i = 0; i < plant_count; i++)
        if (plants[i].active && plants[i].tag_id == tag_id)
            return &plants[i];
    return NULL;
}

static plant_state_t *add_plant(uint8_t tag_id, uint8_t profile_id)
{
    if (plant_count >= MAX_PLANTS) return NULL;
    plant_state_t *p = &plants[plant_count++];
    memset(p, 0, sizeof(*p));
    p->active = 1;
    p->tag_id = tag_id;
    p->profile_id = profile_id;
    p->hours_to_water = 0xFFFF;
    return p;
}

/* ---- Compute per-plant status + risk ---- */
static void compute_plant_status(plant_state_t *p)
{
    const gp_plant_profile_t *prof = gp_get_profile(p->profile_id);
    if (!prof) {
        p->status = GP_PLANT_STRESS;
        return;
    }

    float moisture = (float)p->soil_moisture;
    float temp = (float)p->temp_centic / 100.0f;
    float humidity = (float)p->humidity_centi / 100.0f;

    /* Water risk: based on moisture vs threshold + drying rate */
    if (moisture < (float)prof->min_moisture) {
        p->status = GP_PLANT_WATER_NOW;
        p->water_risk = 100;
        p->hours_to_water = 0;
    } else if (p->dry_rate_pct_per_hr > 0) {
        float hours_left = (moisture - (float)prof->min_moisture) /
                           p->dry_rate_pct_per_hr;
        p->hours_to_water = (uint16_t)hours_left;
        if (hours_left < 48.0f) {
            p->status = GP_PLANT_WATER_SOON;
            p->water_risk = (uint8_t)(100.0f * (1.0f - hours_left / 48.0f));
        } else {
            p->water_risk = (uint8_t)(100.0f * (1.0f - hours_left / 168.0f));
            if (p->water_risk > 50) p->water_risk = 50;
        }
    } else {
        p->hours_to_water = 0xFFFF;
        p->water_risk = 0;
    }

    /* Overwatering check */
    if (moisture > (float)prof->max_moisture) {
        p->water_risk = 0;
        p->hours_to_water = 0xFFFF;
        /* Could flag overwatering risk separately */
    }

    /* Light risk: DLI proxy (lux * hours, simplified to lux * 0.25 for 6hr avg) */
    float dli_proxy = (float)p->ambient_lux / 10.0f * 6.0f;  /* rough 6-hr daily */
    if (dli_proxy < (float)prof->min_dli_lux_hr / 10.0f) {
        if (p->status == GP_PLANT_OK)
            p->status = GP_PLANT_LOW_LIGHT;
        p->light_risk = (uint8_t)(100.0f * (1.0f - dli_proxy /
                              ((float)prof->min_dli_lux_hr / 10.0f)));
    } else {
        p->light_risk = 0;
    }

    /* Temperature stress */
    if (temp < (float)prof->temp_min_centic / 100.0f ||
        temp > (float)prof->temp_max_centic / 100.0f) {
        if (p->status == GP_PLANT_OK)
            p->status = GP_PLANT_STRESS;
    }

    /* Disease risk: if flags indicate scanner found disease */
    if (p->flags & (GP_ALERT_DISEASE_SUSPECT | GP_ALERT_PEST_DETECTED)) {
        p->status = GP_PLANT_DISEASE;
        p->disease_risk = 80;
    }

    /* Disease risk from environment (edge model) */
    if (p->status != GP_PLANT_DISEASE) {
        float input[MODEL_INPUT_DIM];
        memset(input, 0, sizeof(input));
        input[0] = moisture;
        input[1] = humidity;
        input[2] = temp;
        input[3] = 0;  /* light_var (would compute from ring buffer) */
        input[4] = 0;  /* moisture_std (would compute from ring buffer) */

        float output[3];
        TfLiteMicroModel *m = tflm_model_create(disease_model_data, disease_model_len);
        tflm_model_invoke(m, input, MODEL_INPUT_DIM, output, 3);
        p->disease_risk = (uint8_t)(output[0] * 100.0f);
    }
}

/* ---- Mesh RX callback ---- */
static void mesh_rx_handler(uint8_t type, const uint8_t *data, size_t len)
{
    switch (type) {
    case GP_MSG_TELEMETRY: {
        if (len < sizeof(gp_telemetry_payload_t) - 2) break;
        const gp_telemetry_payload_t *p = (const gp_telemetry_payload_t *)data;
        plant_state_t *plant = find_plant(p->node_id);
        if (!plant)
            plant = add_plant(p->node_id, p->plant_profile_id);
        if (!plant) break;

        /* Update drying-rate learning (EWMA) */
        int64_t now = (int64_t)time_us_64() / 1000;
        if (plant->last_telem_ms > 0) {
            float dt_hr = (float)(now - plant->last_telem_ms) / 3600000.0f;
            if (dt_hr > 0) {
                float dmoist = (float)plant->soil_moisture - (float)p->soil_moisture;
                float rate = dmoist / dt_hr;  /* %/hr (positive = drying) */
                if (rate > 0 && rate < 50)  /* sanity filter */
                    plant->dry_rate_pct_per_hr =
                        plant->dry_rate_pct_per_hr * 0.7f + rate * 0.3f;
            }
        }

        plant->soil_moisture = p->soil_moisture;
        plant->ambient_lux = p->ambient_lux;
        plant->temp_centic = p->temp_centic;
        plant->humidity_centi = p->humidity_centi;
        plant->battery_pct = p->battery_pct;
        plant->profile_id = p->plant_profile_id;
        plant->flags = p->flags;
        plant->last_telem_ms = now;

        compute_plant_status(plant);

        /* Store in ring buffer */
        ring[ring_head].tag_id = p->node_id;
        ring[ring_head].soil_moisture = p->soil_moisture;
        ring[ring_head].ambient_lux = p->ambient_lux;
        ring[ring_head].temp_centic = p->temp_centic;
        ring[ring_head].humidity_centi = p->humidity_centic;
        ring[ring_head].battery_pct = p->battery_pct;
        ring[ring_head].disease_risk = plant->disease_risk;
        ring[ring_head].water_risk = plant->water_risk;
        ring[ring_head].status = plant->status;
        ring[ring_head].timestamp_ms = now;
        ring_head = (ring_head + 1) % RING_SLOTS;
        if (ring_count < RING_SLOTS) ring_count++;
        break;
    }
    case GP_MSG_WATERING_ACK: {
        if (len < sizeof(gp_watering_ack_payload_t) - 2) break;
        const gp_watering_ack_payload_t *p = (const gp_watering_ack_payload_t *)data;
        printf("Watering ack: zone=%u status=%u ml=%u\n",
               p->node_id & 0x0F, p->status, p->ml_delivered);
        /* Mark plant as watered (zone maps to plant; simplified) */
        /* In production: map zone+emitter to plant tag */
        break;
    }
    case GP_MSG_SCAN_RESULT: {
        if (len < sizeof(gp_scan_result_payload_t) - 2) break;
        const gp_scan_result_payload_t *p = (const gp_scan_result_payload_t *)data;
        plant_state_t *plant = find_plant(p->plant_tag_id);
        if (plant) {
            if (p->flags & GP_ALERT_DISEASE_SUSPECT)
                plant->flags |= GP_ALERT_DISEASE_SUSPECT;
            if (p->flags & GP_ALERT_PEST_DETECTED)
                plant->flags |= GP_ALERT_PEST_DETECTED;
            compute_plant_status(plant);
        }
        printf("Scan: tag=%u species=%u disease=%u conf=%u pests=%u\n",
               p->plant_tag_id, p->species_id_lo | (p->species_id_hi << 8),
               p->disease_class, p->disease_conf, p->pest_count);
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

/* ---- Trigger watering for a plant ---- */
static void trigger_watering(plant_state_t *plant)
{
    const gp_plant_profile_t *prof = gp_get_profile(plant->profile_id);
    if (!prof) return;

    /* Compute watering duration based on moisture deficit
     * Target: bring moisture up to max_moisture
     * Simplified: assume 1L/hr drip rate, pot volume ~2L
     * duration = (max - current)% * pot_volume / flow_rate */
    float deficit = (float)prof->max_moisture - (float)plant->soil_moisture;
    if (deficit <= 0) return;
    uint16_t target_ml = (uint16_t)(deficit * 20.0f);  /* 20ml per % deficit */
    uint16_t duration = (uint16_t)(target_ml / 16.67f);  /* 1L/hr = 16.67ml/s */

    printf("Watering tag %u: %u ml over %u s\n", plant->tag_id, target_ml, duration);
    gp_send_watering_cmd(plant->tag_id & 0x07, plant->tag_id, duration, target_ml);
    plant->last_watered_ms = (int64_t)time_us_64() / 1000;
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
    gp_mesh_set_tx(mesh_uart_tx);
    gp_mesh_set_rx_callback(mesh_rx_handler);

    /* Initialize SPI for TFT + SD + Flash */
    spi_init(spi0, 24000000);

    /* Initialize I2C for RTC */
    i2c_init(i2c0, 100000);

    uint32_t last_inference_ms = 0;
    uint32_t last_watering_check_ms = 0;

    while (1) {
        /* Poll mesh UART for incoming frames */
        if (uart_is_readable(MESH_UART)) {
            uint8_t buf[64];
            int n = 0;
            while (uart_is_readable(MESH_UART) && n < 64) {
                buf[n++] = uart_getc(MESH_UART);
            }
            gp_mesh_on_rx(buf, n);
        }

        uint32_t now = (uint32_t)(time_us_64() / 1000);

        /* Watering check every 5 min (300000 ms) */
        if (now - last_watering_check_ms > 300000) {
            last_watering_check_ms = now;
            for (int i = 0; i < plant_count; i++) {
                if (plants[i].active && plants[i].status == GP_PLANT_WATER_NOW) {
                    trigger_watering(&plants[i]);
                }
            }
        }

        /* Edge inference + cloud sync every 15 min (900000 ms) */
        if (now - last_inference_ms > 900000) {
            last_inference_ms = now;

            for (int i = 0; i < plant_count; i++) {
                if (!plants[i].active) continue;
                /* Broadcast stress score to mesh (valve uses it) */
                gp_send_stress_score(plants[i].tag_id, plants[i].disease_risk,
                                     plants[i].water_risk, plants[i].light_risk,
                                     plants[i].status, plants[i].hours_to_water);

                /* Send to cloud */
                char json[256];
                snprintf(json, sizeof(json),
                    "{\"type\":\"plant\",\"tag\":%u,\"soil\":%u,\"lux\":%u,"
                    "\"temp_c\":%.2f,\"humidity\":%.2f,\"batt\":%u,\"status\":%u,"
                    "\"disease_risk\":%u,\"water_risk\":%u,\"light_risk\":%u,"
                    "\"hours_to_water\":%u}",
                    plants[i].tag_id, plants[i].soil_moisture,
                    plants[i].ambient_lux, plants[i].temp_centic / 100.0f,
                    plants[i].humidity_centic / 100.0f, plants[i].battery_pct,
                    plants[i].status, plants[i].disease_risk,
                    plants[i].water_risk, plants[i].light_risk,
                    plants[i].hours_to_water);
                wifi_send_to_cloud(json);
            }
        }

        sleep_ms(10);
    }

    return 0;
}