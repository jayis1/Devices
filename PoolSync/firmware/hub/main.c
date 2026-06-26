/**
 * @file main.c
 * @brief PoolSync Hub firmware — RP2040 + ESP32-S3 dual-core gateway
 *
 * The RP2040 handles:
 *   - SX1262 Sub-GHz radio management (TDMA scheduling)
 *   - Local rules engine (pH dosing, freeze protection, safety interlocks)
 *   - ILI9341 display driver (pool status dashboard)
 *   - UART bridge to ESP32-S3
 *
 * The ESP32-S3 handles:
 *   - Wi-Fi 6 + MQTT cloud bridge
 *   - BLE 5.0 provisioning
 *   - Edge ML inference (clearWater, algaeNet)
 *   - OTA update server for all nodes
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/watchdog.h"
#include "psp_protocol.h"
#include "psp_sensor.h"
#include "psp_radio.h"

/* ============================================================
 * PIN DEFINITIONS — RP2040
 * ============================================================ */

/* SPI0 — SX1262 Sub-GHz Radio */
#define PIN_SX_SPI0_MISO   0   /* GP0 = SPI0 RX */
#define PIN_SX_SPI0_CS      1   /* GP1 = SPI0 CSn */
#define PIN_SX_SPI0_SCK     2   /* GP2 = SPI0 SCK */
#define PIN_SX_SPI0_MOSI    3   /* GP3 = SPI0 TX */
#define PIN_SX_DIO1         4   /* GP4 = SX1262 DIO1 (IRQ) */
#define PIN_SX_BUSY         5   /* GP5 = SX1262 BUSY */
#define PIN_SX_NRST         6   /* GP6 = SX1262 NRST */

/* UART0 — ESP32-S3 bridge */
#define PIN_ESP_TX           8   /* GP8 = UART0 TX */
#define PIN_ESP_RX           9   /* GP9 = UART0 RX */
#define PIN_ESP_RST          10  /* GP10 = ESP32-S3 RST */
#define PIN_ESP_BOOT         11  /* GP11 = ESP32-S3 BOOT0 */

/* SPI1 — ILI9341 Display */
#define PIN_LCD_SPI1_MOSI   14  /* GP14 = SPI1 TX */
#define PIN_LCD_SPI1_SCK    15  /* GP15 = SPI1 SCK */
#define PIN_LCD_CS           16  /* GP16 = SPI1 CSn */
#define PIN_LCD_DC           17  /* GP17 = Data/Command */
#define PIN_LCD_RST          18  /* GP18 = Reset */
#define PIN_LCD_BL           19  /* GP19 = Backlight (PWM) */

/* Debug LED */
#define PIN_LED_PWR          20  /* Green — power */
#define PIN_LED_LINK          21  /* Blue — Sub-GHz link */
#define PIN_LED_ALERT        22  /* Red — alert */

/* ============================================================
 * CONSTANTS
 * ============================================================ */

#define UART_ESP_BAUD        115200
#define MAIN_LOOP_PERIOD_MS  1000   /* 1 second main loop */
#define TDMA_FRAME_MS        1000   /* 1-second TDMA frame */
#define RULES_ENGINE_PERIOD  5      /* Run rules every 5 seconds */
#define CLOUD_UPLOAD_PERIOD  60     /* Upload to cloud every 60 seconds */
#define DISPLAY_UPDATE_MS    500    /* Display refresh every 500ms */

/* Chemistry dosing safety limits */
#define ACID_MAX_DOSE_ML     200   /* Maximum acid dose per cycle */
#define CHLORINE_MAX_DOSE_ML  500   /* Maximum chlorine dose per cycle */
#define CLARIFIER_MAX_DOSE_ML 100  /* Maximum clarifier dose per cycle */

/* ============================================================
 * STATE
 * ============================================================ */

typedef struct {
    /* Latest readings from each node */
    psp_chem_reading_t  chem_readings[3];   /* Up to 3 chemistry probes */
    psp_clarity_reading_t clarity;           /* Camera clarity */
    psp_equip_reading_t equip;              /* Equipment status */
    psp_solar_reading_t solar;              /* Solar monitor */
    psp_heartbeat_t    heartbeats[8];       /* Last heartbeat from each node */

    /* Chemistry dosing state */
    float acid_dosed_ml;          /* Total acid dosed this cycle */
    float chlorine_dosed_ml;      /* Total chlorine dosed this cycle */
    float clarifier_dosed_ml;     /* Total clarifier dosed this cycle */
    uint32_t last_dose_time_s;    /* Timestamp of last dose */

    /* Safety state */
    bool gfci_fault;
    bool entrapment_detected;
    bool unsupervised_access;
    bool freeze_protection_active;
    bool all_pumps_off;            /* Emergency shutdown flag */

    /* System state */
    uint32_t uptime_s;
    uint8_t  num_chem_probes;
    bool     cloud_connected;
    bool     display_on;
} psp_hub_state_t;

static psp_hub_state_t g_state;

/* ============================================================
 * RULES ENGINE — Local chemistry and safety rules
 * ============================================================ */

/**
 * Check if pH needs correction and calculate acid/base dose
 * Pool pH should be 7.2–7.6, ideal 7.4–7.6
 * Acid (muriatic) lowers pH, sodium carbonate raises it
 */
static float rules_calculate_acid_dose(float current_ph, float target_ph)
{
    /* Simplified Langelier saturation index adjustment */
    /* Real implementation: accounts for alkalinity, temperature, TDS */
    if (current_ph <= 7.2f || current_ph >= 8.5f)
        return 0.0f;  /* Out of safe range — manual intervention needed */

    if (current_ph <= target_ph)
        return 0.0f;  /* pH already at or below target */

    /* Rough approximation: 100 mL of 31% muriatic acid lowers 10,000 gal pH by 0.1 */
    float ph_delta = current_ph - target_ph;
    float dose_ml = ph_delta * 100.0f;  /* Scale for ~10,000 gal pool */

    /* Clamp to safety limit */
    if (dose_ml > ACID_MAX_DOSE_ML)
        dose_ml = ACID_MAX_DOSE_ML;

    /* Don't dose if we've already dosed recently (wait 30 min between doses) */
    if (g_state.last_dose_time_s > 0 &&
        (g_state.uptime_s - g_state.last_dose_time_s) < 1800)
        return 0.0f;

    return dose_ml;
}

/**
 * Check if free chlorine needs adjustment
 * Target: 2.0–4.0 ppm for residential pools
 */
static float rules_calculate_chlorine_dose(float current_cl_ppm, float target_cl_ppm)
{
    if (current_cl_ppm >= target_cl_ppm)
        return 0.0f;

    float deficit = target_cl_ppm - current_cl_ppm;
    /* Rough: 100g of 12% liquid chlorine raises 10,000 gal by 1 ppm */
    /* Convert to mL of peristaltic pump (calibrated for specific chlorine) */
    float dose_ml = deficit * 50.0f;  /* Simplified: 50 mL per ppm deficit */

    if (dose_ml > CHLORINE_MAX_DOSE_ML)
        dose_ml = CHLORINE_MAX_DOSE_ML;

    return dose_ml;
}

/**
 * Freeze protection: run pump if water temp drops below threshold
 */
static bool rules_check_freeze_protection(float water_temp_c)
{
    /* Run pump if water temp < 4°C (39°F) to prevent freezing */
    if (water_temp_c < 4.0f && !g_state.freeze_protection_active) {
        g_state.freeze_protection_active = true;
        psp_equip_set_relay(0, true);  /* Turn pump on */
        return true;
    }
    if (water_temp_c >= 5.0f && g_state.freeze_protection_active) {
        g_state.freeze_protection_active = false;
        /* Don't turn pump off if it was on before freeze protection */
    }
    return g_state.freeze_protection_active;
}

/**
 * Safety interlock: shut down all pumps on GFCI or entrapment fault
 */
static void rules_safety_interlock(void)
{
    if (g_state.gfci_fault || g_state.entrapment_detected) {
        /* EMERGENCY: shut everything down */
        for (int i = 0; i < 8; i++) {
            psp_equip_set_relay(i, false);
        }
        g_state.all_pumps_off = true;
    }
}

/* ============================================================
 * DISPLAY — Pool Status Dashboard
 * ============================================================ */

/* ILI9341 commands (simplified) */
#define ILI9341_SWRESET   0x01
#define ILI9341_SLPOUT    0x11
#define ILI9341_DISPON    0x29
#define ILI9341_CASET     0x2A
#define ILI9341_PASET     0x2B
#define ILI9341_RAMWR     0x2C
#define ILI9341_MADCTL    0x36

static void display_init(void)
{
    /* Initialize ILI9341 over SPI1 */
    gpio_put(PIN_LCD_BL, 1);
    /* Full init sequence would be here */
}

static void display_update(const psp_hub_state_t *state)
{
    /* Draw pool status dashboard:
     * - pH: 7.4 ✓ (green) / pH: 6.9 ⚠ (red)
     * - Free Cl: 3.2 ppm ✓
     * - ORP: 720 mV ✓
     * - Water temp: 28°C
     * - Turbidity: 0.5 NTU ✓
     * - Clarity score: 0.92
     * - Algae risk: LOW
     * - Pump: ON | Heater: OFF
     * - Next dose: Acid 0mL | Cl 0mL
     */
    (void)state;
    /* Display rendering code */
}

/* ============================================================
 * SUB-GHz RADIO TDMA SCHEDULER
 * ============================================================ */

static void radio_tdma_schedule(void)
{
    /* Hub transmits in slot 0 (time sync, config, commands)
     * Hub receives in slots 1-7 (node data)
     * Slot 8 is contention for alarms
     * Slot 9 is free/unused
     *
     * Timing is maintained by the hub's RP2040 timer.
     * Nodes synchronize via PSP_MSG_TIME_SYNC.
     */
}

static void radio_process_incoming(const psp_frame_t *frame)
{
    /* Route incoming frames based on message type */
    switch (frame->header.msg_type) {
        case PSP_MSG_CHEM_DATA: {
            /* Store chemistry reading */
            uint16_t src = frame->header.src_addr;
            int probe_idx = src - PSP_ADDR_CHEM_PROBE_BASE;
            if (probe_idx >= 0 && probe_idx < 3) {
                psp_chem_data_t *cd = (psp_chem_data_t *)frame->payload;
                g_state.chem_readings[probe_idx].ph = cd->ph;
                g_state.chem_readings[probe_idx].orp_mv = cd->orp_mv;
                g_state.chem_readings[probe_idx].free_cl_ppm = cd->free_cl_ppm;
                g_state.chem_readings[probe_idx].temperature_c = cd->temperature_c;
                g_state.chem_readings[probe_idx].conductivity_us = cd->conductivity_us;
                g_state.chem_readings[probe_idx].turbidity_ntu = cd->turbidity_ntu;
                g_state.chem_readings[probe_idx].valid_mask = 0x3F; /* All valid */
            }
            break;
        }
        case PSP_MSG_EQUIP_STATUS: {
            psp_equip_status_t *es = (psp_equip_status_t *)frame->payload;
            /* Update equipment state from equipment controller */
            (void)es;
            break;
        }
        case PSP_MSG_IMAGE_DATA: {
            psp_image_data_t *img = (psp_image_data_t *)frame->payload;
            g_state.clarity.clarity_score = img->clarity_score;
            g_state.clarity.green_channel = img->green_channel;
            g_state.clarity.turbidity_ntu = img->turbidity_ntu;
            g_state.clarity.algae_risk = img->algae_risk;
            /* Request full image upload over Wi-Fi if algae_risk > 1 */
            if (img->algae_risk >= 2) {
                /* Send PSP_MSG_IMAGE_UPLOAD to camera node */
            }
            break;
        }
        case PSP_MSG_ALARM: {
            psp_alarm_payload_t *alarm = (psp_alarm_payload_t *)frame->payload;
            /* Handle alarm immediately */
            if (alarm->alarm_type == PSP_ALARM_ENTRAPMENT)
                g_state.entrapment_detected = true;
            else if (alarm->alarm_type == PSP_ALARM_GFCI_FAULT)
                g_state.gfci_fault = true;
            else if (alarm->alarm_type == PSP_ALARM_UNAUTH_ACCESS)
                g_state.unsupervised_access = true;
            /* Run safety interlock */
            rules_safety_interlock();
            break;
        }
        case PSP_MSG_HEARTBEAT: {
            psp_heartbeat_t *hb = (psp_heartbeat_t *)frame->payload;
            uint8_t idx = frame->header.src_addr & 0xFF;
            if (idx < 8)
                g_state.heartbeats[idx] = *hb;
            break;
        }
        default:
            break;
    }
}

/* ============================================================
 * ESP32-S3 UART BRIDGE
 * ============================================================ */

static void esp_bridge_init(void)
{
    uart_init(uart0, UART_ESP_BAUD);
    gpio_set_function(PIN_ESP_TX, GPIO_FUNC_UART);
    gpio_set_function(PIN_ESP_RX, GPIO_FUNC_UART);
}

static void esp_bridge_send_state(const psp_hub_state_t *state)
{
    /* Serialize current state and send to ESP32-S3 over UART
     * ESP32-S3 will forward to cloud via MQTT
     */
    uint8_t buf[256];
    int len = snprintf((char *)buf, sizeof(buf),
        "{\"ph\":%.2f,\"cl\":%.2f,\"orp\":%.0f,\"temp\":%.1f,\"turb\":%.1f,"
        "\"clarity\":%.2f,\"algae_risk\":%d,\"pump\":%s,\"heater\":%s,"
        "\"freeze\":%s,\"gfci\":%s,\"uptime\":%lu}\r\n",
        state->chem_readings[0].ph,
        state->chem_readings[0].free_cl_ppm,
        state->chem_readings[0].orp_mv,
        state->chem_readings[0].temperature_c,
        state->chem_readings[0].turbidity_ntu,
        state->clarity.clarity_score,
        state->clarity.algae_risk,
        state->equip.pump_on ? "true" : "false",
        state->equip.heater_on ? "true" : "false",
        state->freeze_protection_active ? "true" : "false",
        state->gfci_fault ? "true" : "false",
        (unsigned long)state->uptime_s);

    uart_write_blocking(uart0, buf, len);
}

/* ============================================================
 * MAIN
 * ============================================================ */

int main(void)
{
    stdio_init_all();

    /* Initialize GPIO */
    gpio_init(PIN_LED_PWR);
    gpio_init(PIN_LED_LINK);
    gpio_init(PIN_LED_ALERT);
    gpio_set_dir(PIN_LED_PWR, GPIO_OUT);
    gpio_set_dir(PIN_LED_LINK, GPIO_OUT);
    gpio_set_dir(PIN_LED_ALERT, GPIO_OUT);
    gpio_put(PIN_LED_PWR, 1);

    /* Initialize SPI0 for SX1262 */
    spi_init(spi0, 2000000);  /* 2 MHz SPI clock */
    gpio_set_function(PIN_SX_SPI0_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SX_SPI0_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SX_SPI0_MOSI, GPIO_FUNC_SPI);
    gpio_init(PIN_SX_SPI0_CS);
    gpio_set_dir(PIN_SX_SPI0_CS, GPIO_OUT);
    gpio_put(PIN_SX_SPI0_CS, 1);

    /* Initialize SX1262 radio */
    psp_radio_init();

    /* Initialize UART bridge to ESP32-S3 */
    esp_bridge_init();

    /* Initialize display */
    display_init();

    /* Initialize state */
    memset(&g_state, 0, sizeof(g_state));
    g_state.cloud_connected = false;
    g_state.display_on = true;

    /* Main loop */
    uint32_t last_rules_time = 0;
    uint32_t last_display_time = 0;
    uint32_t last_cloud_time = 0;

    while (true) {
        uint32_t now = to_ms_since_boot(get_absolute_time()) / 1000;
        g_state.uptime_s = now;

        /* 1. Receive Sub-GHz radio frames (TDMA slot-based) */
        psp_frame_t rx_frame;
        if (psp_radio_recv(&rx_frame, 100) == 0) {
            radio_process_incoming(&rx_frame);
            gpio_put(PIN_LED_LINK, 1);
        }

        /* 2. Run rules engine every 5 seconds */
        if (now - last_rules_time >= RULES_ENGINE_PERIOD) {
            last_rules_time = now;

            /* Chemistry rules */
            if (g_state.chem_readings[0].valid_mask & PSP_CHEM_VALID_PH) {
                float acid_dose = rules_calculate_acid_dose(
                    g_state.chem_readings[0].ph, 7.4f);
                if (acid_dose > 0.0f) {
                    /* Send dose command to equipment controller */
                    psp_dose_command_t dose = {
                        .pump_id = 0,  /* Acid pump */
                        .volume_ml = acid_dose,
                        .duration_s = 120,
                        .command_id = now
                    };
                    psp_header_t hdr = {
                        .preamble = PSP_PREAMBLE,
                        .sync_word = PSP_SYNC_WORD,
                        .src_addr = PSP_ADDR_HUB,
                        .dst_addr = PSP_ADDR_EQUIP_CTRL,
                        .msg_type = PSP_MSG_DOSE_COMMAND
                    };
                    psp_frame_t tx_frame;
                    psp_encode(&hdr, (uint8_t *)&dose, sizeof(dose),
                               (uint8_t *)&tx_frame, sizeof(tx_frame));
                    psp_radio_send(&tx_frame);
                }
            }

            /* Free chlorine rules */
            if (g_state.chem_readings[0].valid_mask & PSP_CHEM_VALID_CL) {
                float cl_dose = rules_calculate_chlorine_dose(
                    g_state.chem_readings[0].free_cl_ppm, 3.0f);
                if (cl_dose > 0.0f) {
                    psp_dose_command_t dose = {
                        .pump_id = 1,  /* Chlorine pump */
                        .volume_ml = cl_dose,
                        .duration_s = 180,
                        .command_id = now
                    };
                    psp_header_t hdr = {
                        .preamble = PSP_PREAMBLE,
                        .sync_word = PSP_SYNC_WORD,
                        .src_addr = PSP_ADDR_HUB,
                        .dst_addr = PSP_ADDR_EQUIP_CTRL,
                        .msg_type = PSP_MSG_DOSE_COMMAND
                    };
                    psp_frame_t tx_frame;
                    psp_encode(&hdr, (uint8_t *)&dose, sizeof(dose),
                               (uint8_t *)&tx_frame, sizeof(tx_frame));
                    psp_radio_send(&tx_frame);
                }
            }

            /* Freeze protection */
            if (g_state.chem_readings[0].valid_mask & PSP_CHEM_VALID_TEMP) {
                rules_check_freeze_protection(g_state.chem_readings[0].temperature_c);
            }
        }

        /* 3. Update display every 500ms */
        if (now - last_display_time >= (DISPLAY_UPDATE_MS / 1000)) {
            last_display_time = now;
            display_update(&g_state);
        }

        /* 4. Send state to ESP32-S3 (cloud bridge) every 60s */
        if (now - last_cloud_time >= CLOUD_UPLOAD_PERIOD) {
            last_cloud_time = now;
            esp_bridge_send_state(&g_state);
        }

        /* 5. Watchdog kick */
        watchdog_update();
    }

    return 0;
}