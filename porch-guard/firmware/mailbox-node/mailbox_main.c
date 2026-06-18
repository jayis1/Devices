/*
 * mailbox_main.c — PorchGuard Mailbox Node (STM32L011K4)
 *
 * Responsibilities:
 * - Ultra-low-power deep sleep (<5µA) — wakes on reed switch (door open) or
 *   periodic 5-min temperature/light poll
 * - Mail classification by weight (HX711 load cell):
 *     <20g = letter, 20-200g = thick, >200g = parcel
 * - Door-open detection (reed switch)
 * - Mailbox temperature (DS18B20) — heat damage to parcels
 * - Tamper detection (LIS2DH12 accel — smash-and-grab / fishing)
 * - Long-range LoRa uplink (SX1261 SF9/SF12) to hub
 * - Solar-aware polling: boost rate in daylight, deep sleep at night
 * - Low-battery alert at 15%
 *
 * Power: 2× CR2032 (3V) + 0.5W solar + MCP73831 trickle
 * Lifetime: months+ (solar tops up during daylight)
 */

#include <stdio.h>
#include <string.h>
#include "stm32l0xx.h"
#include "mesh_protocol.h"

/* ---- Pin Definitions (STM32L011K4) ---- */
#define PIN_SX1261_SPI         SPI1
#define PIN_SX1261_SCK         GPIO_PIN_8   /* PA8 */
#define PIN_SX1261_MOSI        GPIO_PIN_7   /* PA7 */
#define PIN_SX1261_MISO        GPIO_PIN_6   /* PA6 */
#define PIN_SX1261_CS          GPIO_PIN_5   /* PA5 */
#define PIN_SX1261_BUSY        GPIO_PIN_0   /* PB0 */
#define PIN_SX1261_IRQ         GPIO_PIN_1   /* PB1 */
#define PIN_SX1261_NRST        GPIO_PIN_2   /* PB2 */

#define PIN_REED_DOOR          GPIO_PIN_0   /* PA0 — EXTI wake source */
#define PIN_HX711_SCK          GPIO_PIN_1   /* PA1 */
#define PIN_HX711_DOUT         GPIO_PIN_2   /* PA2 */
#define PIN_DS18B20             GPIO_PIN_3   /* PA3 — 1-wire */
#define PIN_ALS_PT19            GPIO_PIN_4   /* PA4 — ADC (light) */
#define PIN_SOLAR_ADC           GPIO_PIN_9   /* PA9 — ADC (solar voltage) */
#define PIN_BATT_ADC            GPIO_PIN_10  /* PA10 — ADC (battery) */
#define PIN_LIS2DH_INT1        GPIO_PIN_11   /* PA11 — accel IRQ (wake source) */

/* ---- Wake sources ---- */
/* Wake on:
 *   - Reed switch (door open) — EXTI rising edge
 *   - LIS2DH12 INT1 (tilt/shock) — EXTI rising edge
 *   - RTC periodic (every 5 min) for temp/light/battery poll
 */

/* ---- HX711 load cell (24-bit) ---- */
static int32_t hx711_read_raw(void)
{
    /* In production:
     * - Wait for DOUT to go low (data ready)
     * - Clock 24 pulses to read 24-bit value
     * - Convert to signed 32-bit, apply calibration offset + scale
     * Returns weight in milligrams after calibration.
     *
     * Stub: simulate a letter (12g = 12000mg) */
    return 12000;
}

static uint8_t classify_mail(int32_t weight_mg)
{
    if (weight_mg < 20)       return MAIL_EMPTY;
    if (weight_mg < 20000)    return MAIL_LETTER;    /* <20g */
    if (weight_mg < 200000)   return MAIL_THICK;      /* 20-200g */
    return MAIL_PARCEL;                              /* >200g */
}

/* ---- DS18B20 temperature (stub) ---- */
static int16_t read_temp_c_x10(void)
{
    /* In production: 1-wire read DS18B20 scratchpad, convert to °C×10.
     * Stub: 25.0°C */
    return 250;
}

/* ---- Light sensor (ALS-PT19, ADC) ---- */
static uint16_t read_light_lux(void)
{
    /* In production: ADC read, convert to lux via calibration.
     * Stub: 1500 lux (daytime) */
    return 1500;
}

/* ---- Battery voltage (ADC) ---- */
static uint8_t read_battery_pct(void)
{
    /* In production: ADC read, map 2.0V→0%, 3.0V→100% (2× CR2032).
     * Stub: 85% */
    return 85;
}

static uint8_t read_solar_mv_x10(void)
{
    /* Solar panel voltage ×10 mV. Stub: 2500mV → 250 */
    return 250;
}

/* ---- LIS2DH12 accelerometer (tamper) ---- */
static uint8_t check_tamper(void)
{
    /* In production: read LIS2DH12 INT1 source register:
     *   - activity/inactivity threshold exceeded → tilt
     *   - shock/high-g → smash-and-grab
     * Returns 0=ok, 1=tilt, 2=forced-open.
     * Stub: 0 (no tamper) */
    return 0;
}

/* ---- SX1261 radio (stub) ---- */
static void sx1261_init(void)
{
    /* Configure SPI1 for SX1261, 915MHz.
     * Normal poll: SF9 (long range, ~200-500m)
     * Tamper alert: SF12 (max range + robustness) */
    printf("[SX1261] Initialized (915MHz, SF9/SF12)\n");
}

static void sx1261_send_sf(uint8_t sf, const uint8_t *data, uint16_t len)
{
    /* In production: set SF via LoRa modem config, write FIFO, TX.
     * Stub: print. */
    printf("[SX1261] TX SF%d %d bytes\n", sf, len);
}

/* ---- Build + send mailbox data packet ---- */
static void send_mailbox_data(uint8_t last_event, uint16_t event_age_s)
{
    mailbox_data_payload_t payload = {0};
    payload.door_state   = 0;  /* read reed */
    payload.mail_class   = classify_mail(hx711_read_raw());
    payload.weight_mg    = (uint16_t)hx711_read_raw();
    payload.temp_c_x10  = read_temp_c_x10();
    payload.light_lux   = read_light_lux();
    payload.tamper_flag = check_tamper();
    payload.battery_pct = read_battery_pct();
    payload.solar_mv    = read_solar_mv_x10();
    payload.signal_rssi = 0;
    payload.last_event  = last_event;
    payload.event_age_s = event_age_s;

    mesh_packet_t pkt;
    uint16_t len = mesh_build_packet(NODE_ID_MAILBOX, NODE_ID_HUB,
                                      PKT_MAILBOX_DATA,
                                      (uint8_t *)&payload, sizeof(payload), &pkt);
    /* Use SF9 for routine telemetry (longer range from curb) */
    sx1261_send_sf(9, (uint8_t *)&pkt, len);
}

static void send_tamper_alert(uint8_t tamper_type, uint8_t severity)
{
    tamper_alert_payload_t ta = {0};
    ta.alert_level = severity >= 3 ? ALERT_EMERGENCY : ALERT_CRITICAL;
    ta.tamper_type = tamper_type;  /* 2=forced-open, 4=fishing */
    ta.source_node = NODE_ID_MAILBOX;
    ta.severity = severity;

    mesh_packet_t pkt;
    uint16_t len = mesh_build_packet(NODE_ID_MAILBOX, NODE_ID_BROADCAST,
                                      PKT_TAMPER_ALERT,
                                      (uint8_t *)&ta, sizeof(ta), &pkt);
    /* Tamper alert: SF12 (max range + robustness — must reach hub) */
    sx1261_send_sf(12, (uint8_t *)&pkt, len);
    printf("[ALERT] Sent TAMPER_ALERT type=%d SF12\n", tamper_type);
}

/* ---- Event tracking ---- */
static uint32_t last_event_time_ms = 0;
static uint8_t  last_event_type = 0;  /* 0=none,1=mail-arrived,2=mail-collected,3=tamper */

static uint16_t event_age_seconds(void)
{
    uint32_t now = 0;  /* in production: read RTC */
    return (uint16_t)((now - last_event_time_ms) / 1000);
}

/* ---- Main loop ----
 * The STM32L0 spends almost all its time in STOP mode (<5µA).
 * It wakes on:
 *   1. Reed switch edge (door open/close) → weigh + classify + send
 *   2. LIS2DH12 INT1 (tilt/shock) → tamper alert
 *   3. RTC wakeup every 5 min → poll temp/light/battery, send if changed
 *
 * Below is the event-driven pseudocode flow.
 */

int main(void)
{
    /* HAL init, clock to HSI 16MHz, configure GPIO + EXTI + ADC + SPI */
    printf("=== PorchGuard Mailbox Node v1.0 ===\n");
    printf("STM32L011K4 + SX1261 + HX711\n");

    sx1261_init();

    /* Configure wake sources:
     *   - EXTI0 (reed, PA0) rising/falling
     *   - EXTI11 (LIS2DH12 INT1, PA11) rising
     *   - RTC alarm every 5 min
     */
    /* HAL_EXTI_EnableRising... */

    uint32_t loop_count = 0;

    while (1) {
        uint16_t age = event_age_seconds();

        /* Door-open event (reed switch) → mail arrival or collection */
        int reed_state = 0;  /* in production: HAL_GPIO_ReadPin(PIN_REED_DOOR) */
        if (reed_state == 1 && last_event_type != 1) {
            /* Door just opened — weigh mail */
            int32_t weight = hx711_read_raw();
            uint8_t mail_class = classify_mail(weight);
            printf("[EVENT] Door opened: mail=%d weight=%dmg\n", mail_class, weight);

            if (mail_class != MAIL_EMPTY) {
                last_event_type = 1;  /* mail-arrived */
                last_event_time_ms = loop_count * 1000;
                send_mailbox_data(1, 0);
            } else {
                /* Door opened but no mail weight = mail was collected */
                last_event_type = 2;  /* mail-collected */
                last_event_time_ms = loop_count * 1000;
                send_mailbox_data(2, 0);
            }
        }

        /* Tamper (LIS2DH12) → instant alert */
        uint8_t tamper = check_tamper();
        if (tamper != 0) {
            printf("[ALERT] Tamper detected: %d\n", tamper);
            last_event_type = 3;
            last_event_time_ms = loop_count * 1000;
            /* tamper_type: 2=forced-open, 4=fishing (upside-down shake) */
            send_tamper_alert(tamper == 2 ? 2 : 4, 3);
        }

        /* Periodic poll every 5 min (RTC wake): temp, light, battery */
        if ((loop_count % 300) == 0) {
            send_mailbox_data(last_event_type, age);
            printf("[POLL] Temp=%.1f°C light=%dlux bat=%d%%\n",
                   read_temp_c_x10() / 10.0f, read_light_lux(), read_battery_pct());

            /* Low-battery alert */
            if (read_battery_pct() < 15) {
                printf("[WARN] Low battery: %d%%\n", read_battery_pct());
            }

            /* High-temp alert (parcel heat damage) */
            if (read_temp_c_x10() > 500) {  /* >50°C */
                printf("[WARN] Mailbox hot: %.1f°C — parcel at risk\n",
                       read_temp_c_x10() / 10.0f);
            }
        }

        /* Solar-aware: if dark (light <100 lux), extend poll interval to 10 min */
        if (read_light_lux() < 100) {
            /* In production: set RTC alarm to 10 min, deep sleep longer */
        }

        loop_count++;

        /* Enter STOP mode (in production: HAL_PWR_EnterSTOPMode)
         * Wakes on EXTI (reed/accel) or RTC alarm.
         * Current: <5µA. */
        /* HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI); */
    }

    return 0;
}