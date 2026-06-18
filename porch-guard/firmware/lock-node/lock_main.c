/*
 * lock_main.c — PorchGuard Lock Node (nRF52840)
 *
 * Responsibilities:
 * - BLE 5.0 GATT peripheral: phone unlocks via app, hub unlocks via BLE cmd
 * - Motorized deadbolt (stepper + A4988) — auto-lock/unlock
 * - 12-key capacitive keypad — PIN entry + one-time courier codes
 * - Garage door relay — fire opener for secure parcel drop
 * - Door sensor (reed switch) — open/closed, left-open alert
 * - Tamper detection (LIS2DH12 accel — forced entry / lock removal)
 * - Auto-lock: re-lock 30s after unlock if door closed
 * - One-time codes: time-limited 6-digit codes tied to a delivery
 * - Keypad anti-shoulder-surf: random digit scramble on wake
 * - Sub-GHz fallback (SX1261) when out of BLE range
 *
 * Power: 4× AA (6V) → AP2112-3.3. Lifetime: 8-12 months.
 * System-off RAM <3µA between events.
 */

#include <stdio.h>
#include <string.h>
#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_drv_spi.h"
#include "ble.h"
#include "mesh_protocol.h"

/* ---- Pin Definitions (nRF52840) ---- */
#define PIN_STEPPER_STEP    3   /* A4988 STEP */
#define PIN_STEPPER_DIR     4   /* A4988 DIR */
#define PIN_STEPPER_EN      5   /* A4988 ENABLE (active low) */
#define PIN_STEPPER_MS1     6   /* microstep */

#define PIN_RELAY_GARAGE    7   /* garage door opener relay */
#define PIN_RELAY_GARAGE_EN 8

#define PIN_REED_DOOR       9   /* door open/closed sensor */
#define PIN_LIS2DH_INT1     10  /* accel IRQ (tamper) */
#define PIN_LIS2DH_INT2     11

#define PIN_SX1261_CS       12
#define PIN_SX1261_IRQ      13
#define PIN_SX1261_BUSY     14
#define PIN_SX1261_NRST     15
/* SPI: SCK=26, MOSI=27, MISO=28 (NRF default) */

#define PIN_KEYPAD_ROWS     {16, 17, 18, 19}   /* 4 rows */
#define PIN_KEYPAD_COLS     {20, 21, 22, 23}   /* wait — 12-key is 3x4 */
/* Keypad: 3 cols × 4 rows = 12 keys */
#define PIN_KEYPAD_COL0     20
#define PIN_KEYPAD_COL1     21
#define PIN_KEYPAD_COL2     22
#define PIN_KEYPAD_ROW0     23
#define PIN_KEYPAD_ROW1     24
#define PIN_KEYPAD_ROW2     25
#define PIN_KEYPAD_ROW3     26

#define PIN_BATT_ADC         30  /* battery voltage divider */
#define PIN_LED_STATUS       31
#define PIN_BUZZER           32  /* keypad feedback */

/* ---- Lock state ---- */
typedef enum {
    LOCK_LOCKED = 0,
    LOCK_UNLOCKED = 1,
    LOCK_ERROR = 2
} lock_state_t;

static lock_state_t lock_state = LOCK_LOCKED;
static uint8_t door_state = 0;       /* 0=closed, 1=open */
static uint8_t tamper_flag = 0;
static uint8_t battery_pct = 100;
static uint8_t auto_lock_enabled = 1;
static uint8_t auto_lock_delay_s = 30;
static uint32_t unlock_time_ms = 0;
static uint8_t last_unlock_src = 0;
static uint8_t last_code_id = 0;
static uint8_t garage_relay_on = 0;
static uint8_t keypad_active = 0;
static uint8_t codes_active = 0;

/* ---- One-time code slots (stored in flash) ---- */
#define MAX_CODE_SLOTS 8
typedef struct {
    uint8_t  code_id;
    uint8_t  digits[6];
    uint16_t valid_window_s;  /* seconds from issue */
    uint32_t issued_at_s;
    uint8_t  used;             /* 0=available, 1=used */
    uint8_t  active;
} code_slot_t;
static code_slot_t code_slots[MAX_CODE_SLOTS];

/* ---- Stepper motor (deadbolt) ---- */
static void motor_init(void)
{
    nrf_gpio_cfg_output(PIN_STEPPER_STEP);
    nrf_gpio_cfg_output(PIN_STEPPER_DIR);
    nrf_gpio_cfg_output(PIN_STEPPER_EN);
    nrf_gpio_cfg_output(PIN_STEPPER_MS1);
    nrf_gpio_pin_set(PIN_STEPPER_EN);    /* disabled (active low) */
    nrf_gpio_pin_set(PIN_STEPPER_MS1);   /* 1/2 microstep */
}

static void motor_drive(uint16_t steps, uint8_t dir)
{
    /* Drive A4988 stepper for `steps` half-steps in direction `dir` (0=lock,1=unlock)
     * Step pulse: 200µS high, 2ms between pulses
     * A full deadbolt throw = ~32 half-steps (depends on mechanical design)
     * Peak current ~250mA for <1s */
    nrf_gpio_pin_clear(PIN_STEPPER_EN);  /* enable */
    nrf_gpio_pin_write(PIN_STEPPER_DIR, dir);

    for (uint16_t i = 0; i < steps; i++) {
        nrf_gpio_pin_set(PIN_STEPPER_STEP);
        nrf_delay_us(200);
        nrf_gpio_pin_clear(PIN_STEPPER_STEP);
        nrf_delay_ms(2);
    }
    nrf_gpio_pin_set(PIN_STEPPER_EN);    /* disable (save power) */
}

static void do_lock(void)
{
    printf("[LOCK] Locking deadbolt...\n");
    motor_drive(32, 0);  /* 32 half-steps, dir=lock */
    lock_state = LOCK_LOCKED;
    last_unlock_src = 0;
}

static void do_unlock(uint8_t source, uint8_t code_id)
{
    printf("[LOCK] Unlocking deadbolt (source=%d code=%d)...\n", source, code_id);
    motor_drive(32, 1);  /* 32 half-steps, dir=unlock */
    lock_state = LOCK_UNLOCKED;
    last_unlock_src = source;
    last_code_id = code_id;
    unlock_time_ms = 0;  /* in production: read RTC */

    /* Mark one-time code as used */
    if (source == UNLOCK_COURIER && code_id > 0) {
        for (int i = 0; i < MAX_CODE_SLOTS; i++) {
            if (code_slots[i].code_id == code_id && code_slots[i].active) {
                code_slots[i].used = 1;
                codes_active--;
                printf("[CODE] Code %d marked used\n", code_id);
                break;
            }
        }
    }

    /* Short beep for feedback */
    nrf_gpio_pin_set(PIN_BUZZER);
    nrf_delay_ms(80);
    nrf_gpio_pin_clear(PIN_BUZZER);
}

static void garage_relay_pulse(uint8_t duration_s)
{
    printf("[GARAGE] Relay pulse %ds\n", duration_s);
    nrf_gpio_pin_set(PIN_RELAY_GARAGE_EN);
    nrf_gpio_pin_set(PIN_RELAY_GARAGE);
    garage_relay_on = 1;
    nrf_delay_ms(duration_s * 1000);  /* in production: use timer */
    nrf_gpio_pin_clear(PIN_RELAY_GARAGE);
    nrf_gpio_pin_clear(PIN_RELAY_GARAGE_EN);
    garage_relay_on = 0;
}

/* ---- Keypad (3×4 capacitive) ---- */
static char keypad_scan(void)
{
    /* In production:
     * 1. Set col0 high, read rows → key 1,2,3
     * 2. Set col1 high, read rows → key 4,5,6
     * 3. Set col2 high, read rows → key 7,8,9,*,0,#
     * Anti-shoulder-surf: display scrambled labels on wake.
     *
     * Stub: return '\0' (no key) */
    return '\0';
}

static uint8_t read_keypad_code(char *out_code, uint8_t max_len)
{
    /* Read up to max_len digits from keypad, terminated by '#'.
     * Returns number of digits entered, 0 if timeout.
     * In production: scan with debounce, timeout 15s. */
    keypad_active = 1;
    uint8_t count = 0;
    /* ... scan loop ... */
    keypad_active = 0;
    return count;
}

static int8_t validate_code(const char *code, uint8_t len)
{
    /* Validate against:
     *   1. Master PIN (resident)
     *   2. Active one-time courier codes (check window + used flag)
     * Returns: -1=invalid, 0=master, >0=code_id of courier code */
    return -1;  /* stub */
}

/* ---- LIS2DH12 tamper detection ---- */
static uint8_t check_tamper(void)
{
    /* Read LIS2DH12 INT1: activity threshold (tilt) or shock detection.
     * Also detect motor back-EMF anomaly (forced turn of deadbolt).
     * Returns 0=ok, 1=tilt, 2=motor-anomaly.
     * Stub: 0 */
    return 0;
}

/* ---- Door sensor ---- */
static uint8_t read_door_state(void)
{
    return nrf_gpio_pin_read(PIN_REED_DOOR) ? 1 : 0;
}

/* ---- Battery ---- */
static uint8_t read_battery_pct(void)
{
    /* In production: ADC read, map 4.0V→0%, 6.0V→100% (4×AA).
     * Stub: 92% */
    return 92;
}

/* ---- BLE GATT (stub) ---- */
static void ble_init(void)
{
    /* Configure BLE 5.0 peripheral:
     *   - Custom service UUID for PorchGuard Lock
     *   - Characteristics: unlock, lock, garage, code-issue, code-revoke, status
     *   - LE Secure Connections encryption
     *   - Advertising: 100ms when active, 1s when idle
     * Bonding with phone + hub at setup. */
    printf("[BLE] GATT peripheral initialized\n");
}

static void ble_advertise_start(void)
{
    printf("[BLE] Advertising started\n");
}

/* In production: BLE event handler receives commands from phone/hub:
 *   - UNLOCK(source, code_id)  → do_unlock()
 *   - LOCK()                    → do_lock()
 *   - ISSUE_CODE(code_id, window_min, digits[6])
 *   - REVOKE_CODE(code_id)
 *   - GARAGE_RELAY(duration_s)
 *   - AUTO_LOCK(enabled, delay_s)
 *   - ARM/DISARM (hub broadcasts over mesh)
 */

/* ---- Sub-GHz fallback ---- */
static void sx1261_init(void)
{
    /* Configure SPI for SX1261, 915MHz.
     * Used as fallback when out of BLE range from hub. */
    printf("[SX1261] Initialized (915MHz fallback)\n");
}

static void send_lock_data(void)
{
    lock_data_payload_t payload = {0};
    payload.lock_state = lock_state;
    payload.door_state = door_state;
    payload.last_unlock_src = last_unlock_src;
    payload.last_code_id = last_code_id;
    payload.tamper_flag = tamper_flag;
    payload.battery_pct = battery_pct;
    payload.auto_lock_enabled = auto_lock_enabled;
    payload.door_open_s = 0;  /* in production: compute from unlock_time_ms */
    payload.garage_relay_on = garage_relay_on;
    payload.codes_active = codes_active;
    payload.keypad_active = keypad_active;
    payload.signal_rssi = 0;

    mesh_packet_t pkt;
    uint16_t len = mesh_build_packet(NODE_ID_LOCK, NODE_ID_HUB,
                                      PKT_LOCK_DATA,
                                      (uint8_t *)&payload, sizeof(payload), &pkt);
    /* In production: TX via SX1261 (fallback only when BLE unavailable) */
    printf("[MESH] Sending lock data (%d bytes)\n", len);
}

static void send_tamper_alert(uint8_t tamper_type, uint8_t severity)
{
    tamper_alert_payload_t ta = {0};
    ta.alert_level = severity >= 3 ? ALERT_EMERGENCY : ALERT_CRITICAL;
    ta.tamper_type = tamper_type;
    ta.source_node = NODE_ID_LOCK;
    ta.severity = severity;

    mesh_packet_t pkt;
    uint16_t len = mesh_build_packet(NODE_ID_LOCK, NODE_ID_BROADCAST,
                                      PKT_TAMPER_ALERT,
                                      (uint8_t *)&ta, sizeof(ta), &pkt);
    printf("[ALERT] Sent TAMPER_ALERT type=%d severity=%d\n", tamper_type, severity);
}

/* ---- Main loop ---- */

int main(void)
{
    printf("=== PorchGuard Lock Node v1.0 ===\n");
    printf("nRF52840 + A4988 stepper + keypad\n");

    motor_init();
    ble_init();
    sx1261_init();
    ble_advertise_start();

    nrf_gpio_cfg_input(PIN_REED_DOOR, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_input(PIN_LIS2DH_INT1, NRF_GPIO_PIN_PULLDOWN);
    nrf_gpio_cfg_output(PIN_LED_STATUS);
    nrf_gpio_cfg_output(PIN_BUZZER);

    /* Initialize code slots */
    memset(code_slots, 0, sizeof(code_slots));

    uint32_t loop_count = 0;

    while (1) {
        uint32_t now_ms = 0;  /* in production: read RTC */

        /* Read door state */
        door_state = read_door_state();

        /* Check tamper */
        tamper_flag = check_tamper();
        if (tamper_flag != 0) {
            printf("[ALERT] Lock tamper: %d\n", tamper_flag);
            send_tamper_alert(tamper_flag, 3);
            tamper_flag = 0;  /* one-shot */
        }

        /* Auto-lock: re-lock 30s after unlock if door closed */
        if (lock_state == LOCK_UNLOCKED && auto_lock_enabled &&
            door_state == 0 &&
            (now_ms - unlock_time_ms) > (auto_lock_delay_s * 1000)) {
            printf("[LOCK] Auto-locking (door closed, %ds elapsed)\n",
                   auto_lock_delay_s);
            do_lock();
        }

        /* Door-left-open alert: open >2min → alert hub */
        if (door_state == 1 && lock_state == LOCK_UNLOCKED) {
            uint32_t open_duration = now_ms - unlock_time_ms;
            if (open_duration > 120000) {  /* >2min */
                printf("[WARN] Door open >%ds\n", open_duration / 1000);
                send_lock_data();  /* hub will alert app */
            }
        }

        /* Keypad scan (wakes on key press) */
        char code_buf[6];
        uint8_t code_len = read_keypad_code(code_buf, 6);
        if (code_len > 0) {
            int8_t result = validate_code(code_buf, code_len);
            if (result == 0) {
                /* Master PIN */
                do_unlock(UNLOCK_KEYPAD, 0);
            } else if (result > 0) {
                /* Courier code */
                do_unlock(UNLOCK_COURIER, (uint8_t)result);
                /* Auto fire garage relay for parcel drop */
                garage_relay_pulse(1);
            } else {
                /* Invalid — short error beep */
                nrf_gpio_pin_set(PIN_BUZZER);
                nrf_delay_ms(200);
                nrf_gpio_pin_clear(PIN_BUZZER);
                printf("[KEYPAD] Invalid code\n");
            }
        }

        /* Periodic telemetry every 30s (BLE) or on fallback Sub-GHz */
        if ((loop_count % 60) == 0) {
            battery_pct = read_battery_pct();
            send_lock_data();
        }

        /* Low-battery alert */
        if (battery_pct < 15 && (loop_count % 600) == 0) {
            printf("[WARN] Low battery: %d%%\n", battery_pct);
            send_lock_data();
        }

        /* Status LED: green=locked, blue=unlocked, red=tamper */
        if (tamper_flag) {
            nrf_gpio_pin_set(PIN_LED_STATUS);   /* red (in production: RGB) */
        } else if (lock_state == LOCK_UNLOCKED) {
            /* blue — blink slowly */
            nrf_gpio_pin_toggle(PIN_LED_STATUS);
        } else {
            nrf_gpio_pin_set(PIN_LED_STATUS);   /* green */
        }

        /* Sleep until next event (in production: SystemON sleep, <3µA)
         * Wakes on: BLE event, keypad IRQ, reed IRQ, accel IRQ, RTC */
        /* __WFE(); */

        nrf_delay_ms(500);
        loop_count++;
    }

    return 0;
}