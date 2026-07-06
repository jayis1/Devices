/**
 * SightSync Smart Lamp Node — Main Firmware
 *
 * RP2040 (Arduino-Pico) · Sub-GHz 868 MHz (CC1101)
 * TLC5971 16-channel LED driver, VEML7700 ambient light sensor,
 * Rotary encoder + button for manual override.
 *
 * License: MIT
 */

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Encoder.h>
#include <Adafruit_VEML7700.h>
#include "../common/protocol.h"
#include "../common/crc8.h"
#include "tlc5971.h"
#include "cc1101.h"

/* ── Pin definitions ──────────────────────────────────────────────── */

#define PIN_TLC_SCK    0
#define PIN_TLC_MOSI   1
#define PIN_TLC_LATCH  2
#define PIN_TLC_BLANK  3

#define PIN_I2C_SDA    4
#define PIN_I2C_SCL    5

#define PIN_CC1101_CS  6
#define PIN_CC1101_SCK 7
#define PIN_CC1101_MISO 8
#define PIN_CC1101_MOSI 9
#define PIN_CC1101_GDO0 10
#define PIN_CC1101_GDO2 11

#define PIN_ENC_A      12
#define PIN_ENC_B      13
#define PIN_BTN        14
#define PIN_LED        25  /* onboard LED */

/* ── State ────────────────────────────────────────────────────────── */

typedef struct {
    uint16_t target_cct;      /* K (1800-6500) */
    uint8_t  target_brightness; /* 0-100 */
    uint8_t  mode;             /* 0=auto, 1=manual, 2=circadian, 3=reading */
    uint16_t current_cct;
    uint8_t  current_brightness;
    uint16_t ambient_lux;
    bool     manual_override;
} lamp_state_t;

static lamp_state_t s_state;

/* ── Sub-GHz RX callback ───────────────────────────────────────────── */

static void on_subghz_rx(const sightsync_header_t *hdr, const uint8_t *payload)
{
    switch (hdr->msg_type) {
    case MSG_TYPE_CMD_LAMP: {
        const payload_lamp_cmd_t *cmd = (const payload_lamp_cmd_t *)payload;
        s_state.target_cct = cmd->target_cct;
        s_state.target_brightness = cmd->brightness_pct;
        s_state.mode = cmd->mode;
        s_state.manual_override = (cmd->mode == 1);  /* manual mode */
        break;
    }
    case MSG_TYPE_HEARTBEAT:
        /* Sync TDMA timing */
        break;
    default:
        break;
    }
}

/* ── CCT blending: warm-white + cool-white channels ────────────────── */

static void blend_cct(uint16_t cct, uint8_t brightness)
{
    /* WW (warm) and CW (cool) channel mixing:
     * 1800K → 100% WW, 0% CW
     * 6500K → 0% WW, 100% CW
     * Linear interpolation between.
     */
    float ww_frac, cw_frac;
    if (cct <= 1800) {
        ww_frac = 1.0f;
        cw_frac = 0.0f;
    } else if (cct >= 6500) {
        ww_frac = 0.0f;
        cw_frac = 1.0f;
    } else {
        ww_frac = (float)(6500 - cct) / (6500 - 1800);
        cw_frac = 1.0f - ww_frac;
    }

    /* Apply brightness scaling */
    float bright = brightness / 100.0f;
    uint16_t ww_pwm = (uint16_t)(65535 * ww_frac * bright);
    uint16_t cw_pwm = (uint16_t)(65535 * cw_frac * bright);

    /* Set TLC5971 channels:
     * Channels 0-7: warm-white LEDs
     * Channels 8-15: cool-white LEDs
     */
    uint16_t channels[16];
    for (int i = 0; i < 8; i++) channels[i] = ww_pwm;
    for (int i = 8; i < 16; i++) channels[i] = cw_pwm;

    tlc5971_set_all(channels);
    tlc5971_update();
}

/* ── Ambient light feedback (VEML7700) ────────────────────────────── */

static void ambient_feedback_task(void)
{
    static uint32_t last_read = 0;
    uint32_t now = millis();
    if (now - last_read < 1000) return;  /* 1 Hz */
    last_read = now;

    Adafruit_VEML7700 veml;
    /* veml.begin() called once in setup; read here */
    s_state.ambient_lux = veml.readLux(VEML_LUX_AUTO);

    /* Closed-loop: if ambient < target and not manual, boost lamp brightness */
    if (!s_state.manual_override && s_state.mode == 0) {
        /* Target: 500 lux at desk surface */
        uint16_t target_lux = 500;
        if (s_state.ambient_lux < target_lux) {
            uint8_t boost = (uint8_t)((target_lux - s_state.ambient_lux) * 100 / target_lux);
            uint8_t new_brightness = s_state.target_brightness + boost / 2;
            if (new_brightness > 100) new_brightness = 100;
            if (new_brightness != s_state.current_brightness) {
                s_state.current_brightness = new_brightness;
                blend_cct(s_state.current_cct, new_brightness);
            }
        }
    }
}

/* ── Rotary encoder + button (manual override) ────────────────────── */

static Encoder enc(PIN_ENC_A, PIN_ENC_B);
static int32_t s_enc_pos = 0;

static void check_manual_input(void)
{
    int32_t new_pos = enc.read();
    if (new_pos != s_enc_pos) {
        int32_t diff = new_pos - s_enc_pos;
        s_enc_pos = new_pos;

        if (!s_state.manual_override) {
            s_state.manual_override = true;
            s_state.mode = 1;
        }

        /* Encoder adjusts brightness */
        int32_t new_brightness = (int32_t)s_state.target_brightness + diff * 2;
        if (new_brightness < 0) new_brightness = 0;
        if (new_brightness > 100) new_brightness = 100;
        s_state.target_brightness = (uint8_t)new_brightness;
        s_state.current_brightness = (uint8_t)new_brightness;
        blend_cct(s_state.current_cct, s_state.current_brightness);
    }

    /* Button: cycle CCT (warm → neutral → cool) */
    static uint32_t last_btn = 0;
    if (digitalRead(PIN_BTN) == LOW && millis() - last_btn > 200) {
        last_btn = millis();
        s_state.manual_override = true;
        s_state.mode = 1;
        if (s_state.target_cct < 3000) {
            s_state.target_cct = 4500;
        } else if (s_state.target_cct < 5500) {
            s_state.target_cct = 6500;
        } else {
            s_state.target_cct = 1800;
        }
        s_state.current_cct = s_state.target_cct;
        blend_cct(s_state.current_cct, s_state.current_brightness);
    }
}

/* ── Setup ────────────────────────────────────────────────────────── */

static Adafruit_VEML7700 veml;

void setup(void)
{
    Serial.begin(115200);
    Serial.println("SightSync Smart Lamp Node starting...");

    /* Default state */
    s_state.target_cct = 4000;
    s_state.target_brightness = 60;
    s_state.current_cct = 4000;
    s_state.current_brightness = 60;
    s_state.mode = 0;
    s_state.manual_override = false;

    /* Initialize I²C for VEML7700 */
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(400000);
    veml.begin();
    veml.setGain(VEML7700_GAIN_1_8);
    veml.setIntegrationTime(VEML7700_IT_100MS);

    /* Initialize TLC5971 LED driver */
    tlc5971_init(PIN_TLC_SCK, PIN_TLC_MOSI, PIN_TLC_LATCH, PIN_TLC_BLANK);
    blend_cct(s_state.current_cct, s_state.current_brightness);

    /* Initialize CC1101 Sub-GHz radio */
    cc1101_init(PIN_CC1101_CS, PIN_CC1101_SCK, PIN_CC1101_MISO,
                PIN_CC1101_MOSI, PIN_CC1101_GDO0, on_subghz_rx);

    /* Configure button + onboard LED */
    pinMode(PIN_BTN, INPUT_PULLUP);
    pinMode(PIN_LED, OUTPUT);

    Serial.println("Smart Lamp Node initialized.");
}

/* ── Main loop ────────────────────────────────────────────────────── */

void loop(void)
{
    /* Smooth transition toward target */
    if (s_state.current_cct != s_state.target_cct ||
        s_state.current_brightness != s_state.target_brightness) {

        /* Gradual transition (1 step per loop iteration) */
        if (s_state.current_cct < s_state.target_cct) s_state.current_cct += 10;
        if (s_state.current_cct > s_state.target_cct) s_state.current_cct -= 10;
        if (abs(s_state.current_cct - s_state.target_cct) < 10)
            s_state.current_cct = s_state.target_cct;

        if (s_state.current_brightness < s_state.target_brightness) s_state.current_brightness++;
        if (s_state.current_brightness > s_state.target_brightness) s_state.current_brightness--;

        blend_cct(s_state.current_cct, s_state.current_brightness);
    }

    /* Ambient light feedback */
    ambient_feedback_task();

    /* Manual input (encoder + button) */
    check_manual_input();

    delay(20);  /* 50 Hz loop for smooth transitions */
}