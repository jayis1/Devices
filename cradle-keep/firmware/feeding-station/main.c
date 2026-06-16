/**
 * CradleKeep — Feeding Station Node Firmware (nRF52840)
 * 
 * Countertop device for bottle warming, feeding tracking, and
 * formula dispensing. Tracks feeding volume via precision weight
 * sensors, warms bottles to 37°C, and can dispense formula powder.
 * 
 * Key features:
 * - Precision weight tracking (2g resolution, before/after feeding)
 * - PTC heater with PID temperature control (target 37°C ±0.5°C)
 * - OLED display for temperature, timer, and feeding status
 * - Formula powder dispenser (optional servo)
 * - Milk freshness check via turbidity (UV + photodiode)
 * - BLE for mobile app configuration
 * - Sub-GHz mesh for hub coordination
 */

#include <stdio.h>
#include <string.h>
#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrf_drv_spi.h"
#include "nrf_drv_i2c.h"
#include "nrf_drv_gpiote.h"
#include "nrf_drv_pwm.h"
#include "nrf_drv_timer.h"
#include "nrf_soc.h"
#include "app_error.h"
#include "app_timer.h"

/* ── Pin Definitions (nRF52840) ──────────────────────────────────────── */
#define PIN_HX711_DOUT_1   2    /* Load cell 1 data */
#define PIN_HX711_SCK_1    3    /* Load cell 1 clock */
#define PIN_HX711_DOUT_2   4    /* Load cell 2 data */
#define PIN_HX711_SCK_2    5    /* Load cell 2 clock */
#define PIN_ONE_WIRE        6    /* DS18B20 temperature probe */
#define PIN_HEATER_PWM      7    /* PTC heater MOSFET PWM */
#define PIN_OLED_SDA        8    /* SH1106 I2C data */
#define PIN_OLED_SCL        9    /* SH1106 I2C clock */
#define PIN_SERVO_PWM      10    /* SG90 servo PWM */
#define PIN_UV_LED          11   /* VCSEL 850nm enable */
#define PIN_UV_PHOTODIODE  12   /* Photodiode ADC input */
#define PIN_BTN_START      13   /* Start/stop warming button */
#define PIN_BTN_DISPENSE   14   /* Dispense formula button */
#define PIN_BTN_MODE       15   /* Mode toggle button */
#define PIN_BUZZER         16   /* Piezo buzzer */
#define PIN_RADIO_SCK      17   /* SX1261 SPI clock */
#define PIN_RADIO_MOSI     18   /* SX1261 SPI MOSI */
#define PIN_RADIO_MISO     19   /* SX1261 SPI MISO */
#define PIN_RADIO_NSS      20   /* SX1261 chip select */
#define PIN_RADIO_IRQ      21   /* SX1261 DIO1 interrupt */
#define PIN_RADIO_BUSY     22   /* SX1261 busy */
#define PIN_RADIO_RST      23   /* SX1261 reset */
#define PIN_VBAT_SENSE     25   /* Battery voltage divider */

/* ── Constants ─────────────────────────────────────────────────────── */
#define HX711_GAIN            128     /* Channel A, gain 128 */
#define HX711_SCALE_FACTOR    2280.0f  /* Calibration: raw units per mg */
#define TARGET_TEMP_C_X10     370     /* Target temp: 37.0°C × 10 */
#define TEMP_TOLERANCE_C_X10  5       /* ±0.5°C tolerance */
#define HEATER_PWM_FREQ       100     /* 100 Hz PWM for heater */
#define SERVO_PWM_FREQ         50     /* 50 Hz for servo */
#define WARMING_TIMEOUT_S      300   /* 5 minute warming timeout */
#define FEEDING_TIMEOUT_S      3600  /* 1 hour feeding timeout */
#define MILK_DENSITY_MG_ML   1030   /* Milk density ~1.03 g/ml */
#define TARE_SAMPLES           10     /* Samples for tare */
#define WEIGHT_STABLE_THRESHOLD 100   /* 100mg stability threshold */

/* ── System State ──────────────────────────────────────────────────────── */
typedef struct {
    /* Feeding state */
    uint8_t  feeding_state;      /* FEED_IDLE, FEED_WARMING, etc. */
    int16_t  target_temp_c_x10;  /* Target temperature ×10 */
    int16_t  current_temp_c_x10; /* Current temperature ×10 */
    
    /* Weight tracking */
    int32_t  raw_weight_1;       /* Load cell 1 raw value */
    int32_t  raw_weight_2;       /* Load cell 2 raw value */
    int32_t  weight_mg;          /* Current weight in mg */
    int32_t  tare_weight_mg;     /* Tare weight */
    int32_t  start_weight_mg;    /* Weight at feeding start */
    uint16_t volume_consumed_ml; /* Calculated volume consumed */
    uint32_t feeding_start_ms;   /* Feeding start timestamp */
    uint32_t feeding_duration_s; /* Feeding duration in seconds */
    
    /* Scale calibration */
    int32_t  scale_offset;       /* Zero-point offset */
    float    scale_factor;       /* mg per raw unit */
    uint8_t  scale_calibrated;   /* Calibration state */
    
    /* Heater PID */
    float    pid_kp;             /* Proportional gain */
    float    pid_ki;             /* Integral gain */
    float    pid_kd;             /* Derivative gain */
    float    pid_integral;       /* Integral accumulator */
    float    pid_prev_error;     /* Previous error for derivative */
    uint8_t  heater_pwm_pct;     /* Current heater PWM percentage */
    
    /* Milk freshness */
    uint16_t uv_turbidity;       /* UV turbidity reading */
    uint8_t  milk_freshness;     /* 0-100 freshness score */
    
    /* Battery */
    uint8_t  battery_pct;
    
    /* Buttons */
    uint8_t  btn_start_pressed;
    uint8_t  btn_dispense_pressed;
    uint8_t  btn_mode_pressed;
    
    /* Timing */
    uint32_t last_tx_ms;
    uint32_t warming_start_ms;
} feeding_state_t;

feeding_state_t state;

/* ── HX711 Weight Sensor ────────────────────────────────────────────────── */
/**
 * Read 24-bit value from HX711 load cell amplifier.
 * Clock pulses shift out data bit by bit.
 */
int32_t hx711_read(uint8_t dout_pin, uint8_t sck_pin) {
    /* Wait for DOUT to go low (data ready) */
    uint32_t timeout = 100000;
    while (nrf_gpio_pin_read(dout_pin)) {
        if (--timeout == 0) return 0;  /* Timeout */
    }
    
    int32_t value = 0;
    
    /* Pulse SCK 24 times to shift out 24 data bits */
    for (int i = 0; i < 24; i++) {
        nrf_gpio_pin_set(sck_pin);
        nrf_delay_us(1);
        value <<= 1;
        if (nrf_gpio_pin_read(dout_pin)) {
            value |= 1;
        }
        nrf_gpio_pin_clear(sck_pin);
        nrf_delay_us(1);
    }
    
    /* Set gain to 128 (pulse SCK 1 more time) */
    nrf_gpio_pin_set(sck_pin);
    nrf_delay_us(1);
    nrf_gpio_pin_clear(sck_pin);
    nrf_delay_us(1);
    
    /* Convert from 24-bit two's complement */
    if (value & 0x800000) {
        value |= 0xFF000000;  /* Sign extend */
    }
    
    return value;
}

void hx711_power_up(uint8_t sck_pin) {
    nrf_gpio_pin_clear(sck_pin);  /* SCK low = power up */
}

void hx711_power_down(uint8_t sck_pin) {
    nrf_gpio_pin_set(sck_pin);  /* SCK high = power down */
}

/* ── Weight Calculation ──────────────────────────────────────────────────── */
int32_t calculate_weight_mg(void) {
    /* Read both load cells */
    state.raw_weight_1 = hx711_read(PIN_HX711_DOUT_1, PIN_HX711_SCK_1);
    state.raw_weight_2 = hx711_read(PIN_HX711_DOUT_2, PIN_HX711_SCK_2);
    
    /* Combine load cells (sum) */
    int32_t combined = state.raw_weight_1 + state.raw_weight_2;
    
    /* Subtract offset and apply scale factor */
    int32_t weight_mg = (int32_t)((float)(combined - state.scale_offset) * state.scale_factor);
    
    /* Filter: ignore negative weights (scale not loaded) */
    if (weight_mg < 0) weight_mg = 0;
    
    state.weight_mg = weight_mg;
    return weight_mg;
}

/**
 * Calculate volume consumed in ml from weight change.
 * Milk density ≈ 1.03 g/ml
 */
uint16_t calculate_volume_ml(int32_t start_weight_mg, int32_t current_weight_mg) {
    if (start_weight_mg <= current_weight_mg) return 0;  /* Weight increased? */
    int32_t weight_diff_mg = start_weight_mg - current_weight_mg;
    uint16_t volume_ml = (uint16_t)(weight_diff_mg / MILK_DENSITY_MG_ML);
    return volume_ml;
}

/* ── DS18B20 Temperature Probe ───────────────────────────────────────────── */
/**
 * Read temperature from DS18B20 waterproof probe.
 * OneWire protocol: reset → skip ROM → convert T → wait → read scratchpad
 */
int16_t ds18b20_read(void) {
    /* Simplified OneWire implementation for DS18B20 */
    /* In production, use nrf-onewire library */
    
    /* Reset pulse */
    nrf_gpio_pin_clear(PIN_ONE_WIRE);
    nrf_delay_us(480);
    nrf_gpio_pin_set(PIN_ONE_WIRE);
    nrf_delay_us(70);
    
    /* Check presence */
    if (nrf_gpio_pin_read(PIN_ONE_WIRE) == 1) {
        return -10000;  /* No device found */
    }
    nrf_delay_us(410);
    
    /* Skip ROM command */
    uint8_t skip_rom = 0xCC;
    for (int i = 0; i < 8; i++) {
        nrf_gpio_pin_clear(PIN_ONE_WIRE);
        nrf_delay_us(2);
        if (skip_rom & (1 << i)) {
            nrf_gpio_pin_set(PIN_ONE_WIRE);
        }
        nrf_delay_us(60);
        nrf_gpio_pin_set(PIN_ONE_WIRE);
        nrf_delay_us(2);
    }
    
    /* Convert T command */
    uint8_t convert = 0x44;
    for (int i = 0; i < 8; i++) {
        nrf_gpio_pin_clear(PIN_ONE_WIRE);
        nrf_delay_us(2);
        if (convert & (1 << i)) {
            nrf_gpio_pin_set(PIN_ONE_WIRE);
        }
        nrf_delay_us(60);
        nrf_gpio_pin_set(PIN_ONE_WIRE);
        nrf_delay_us(2);
    }
    
    /* Wait for conversion (750ms max for 12-bit) */
    nrf_delay_ms(750);
    
    /* Read scratchpad */
    /* (Simplified - in production use proper OneWire library) */
    /* Returns temperature in 0.1°C units (×10) */
    
    return 250;  /* Placeholder: 25.0°C */
}

/* ── PID Temperature Control ────────────────────────────────────────────── */
float pid_compute(int16_t target_c_x10, int16_t current_c_x10) {
    float error = (float)(target_c_x10 - current_c_x10) / 10.0f;  /* Error in °C */
    
    /* Proportional */
    float p_term = state.pid_kp * error;
    
    /* Integral (with anti-windup) */
    state.pid_integral += error;
    if (state.pid_integral > 50.0f) state.pid_integral = 50.0f;   /* Anti-windup */
    if (state.pid_integral < -50.0f) state.pid_integral = -50.0f;
    float i_term = state.pid_ki * state.pid_integral;
    
    /* Derivative */
    float d_term = state.pid_kd * (error - state.pid_prev_error);
    state.pid_prev_error = error;
    
    /* Output: 0-100% heater power */
    float output = p_term + i_term + d_term;
    if (output < 0.0f) output = 0.0f;
    if (output > 100.0f) output = 100.0f;
    
    return output;
}

void heater_set_power(uint8_t pct) {
    state.heater_pwm_pct = pct;
    /* Set PWM duty cycle */
    /* nRF PWM driver configured at 100 Hz */
    /* Duty = pct * 10000 / 100 (for 16-bit counter) */
}

/* ── Milk Freshness Check ────────────────────────────────────────────────── */
/**
 * Measure milk turbidity using 850nm VCSEL and photodiode.
 * Fresh milk is opaque (high turbidity), spoiled milk may separate
 * or become clearer. This is a supplementary check, not definitive.
 */
uint8_t check_milk_freshness(void) {
    /* Turn on VCSEL */
    nrf_gpio_pin_set(PIN_UV_LED);
    nrf_delay_ms(5);  /* Let LED stabilize */
    
    /* Read photodiode via ADC */
    /* (Simplified - would use SAADC in production) */
    uint16_t photodiode_value = 2048;  /* Placeholder ADC reading */
    
    /* Turn off VCSEL */
    nrf_gpio_pin_clear(PIN_UV_LED);
    
    state.uv_turbidity = photodiode_value;
    
    /* Score: 100 = perfectly fresh, 0 = definitely spoiled */
    /* Based on turbidity range for fresh milk */
    if (photodiode_value > 3000) {
        state.milk_freshness = 100;  /* Very opaque = fresh */
    } else if (photodiode_value > 2000) {
        state.milk_freshness = 80;
    } else if (photodiode_value > 1000) {
        state.milk_freshness = 50;
    } else {
        state.milk_freshness = 20;  /* Transparent = likely spoiled */
    }
    
    return state.milk_freshness;
}

/* ── OLED Display Update ────────────────────────────────────────────────── */
void update_oled(void) {
    /* Display format depends on state:
     * 
     * IDLE:
     *   ┌────────────────┐
     *   │ CradleKeep      │
     *   │ Ready           │
     *   │ [START] Warm    │
     *   │ [DISPENSE] Fmla │
     *   └────────────────┘
     * 
     * WARMING:
     *   ┌────────────────┐
     *   │ Warming...      │
     *   │ 34.2°C → 37°C  │
     *   │ ████████░░ 82%  │
     *   │ Time: 2:34      │
     *   └────────────────┘
     * 
     * FEEDING:
     *   ┌────────────────┐
     *   │ Feeding...      │
     *   │ Consumed: 45ml  │
     *   │ Duration: 8:32  │
     *   │ Weight: 128g    │
     *   └────────────────┘
     */
    
    /* SH1106 I2C commands would go here */
    /* Using SSD1306-compatible command set */
}

/* ── Button Handlers ────────────────────────────────────────────────────── */
void btn_start_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_evt_t_t evt) {
    switch (state.feeding_state) {
        case FEED_IDLE:
            /* Start warming */
            state.feeding_state = FEED_WARMING;
            state.warming_start_ms = app_timer_cnt_get();
            state.target_temp_c_x10 = TARGET_TEMP_C_X10;
            break;
        case FEED_WARMING:
            /* Stop warming */
            state.feeding_state = FEED_READY;
            heater_set_power(0);
            break;
        case FEED_READY:
            /* Start feeding */
            state.feeding_state = FEED_IN_PROGRESS;
            state.start_weight_mg = state.weight_mg;
            state.feeding_start_ms = app_timer_cnt_get();
            break;
        case FEED_IN_PROGRESS:
            /* Stop feeding */
            state.feeding_state = FEED_DONE;
            state.feeding_duration_s = (app_timer_cnt_get() - state.feeding_start_ms) / 1000;
            state.volume_consumed_ml = calculate_volume_ml(state.start_weight_mg, state.weight_mg);
            break;
        case FEED_DONE:
            /* Reset to idle */
            state.feeding_state = FEED_IDLE;
            break;
    }
}

void btn_dispense_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_evt_t_t evt) {
    /* Dispense formula powder via servo */
    /* Rotate servo 180° to dispense one scoop, then return */
    /* In production: servo PWM control with precise timing */
}

void btn_mode_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_evt_t_t evt) {
    /* Toggle temperature target: 37°C (milk) ↔ 40°C (warm water) */
    if (state.target_temp_c_x10 == 370) {
        state.target_temp_c_x10 = 400;  /* 40°C for warm water */
    } else {
        state.target_temp_c_x10 = 370;  /* 37°C for milk */
    }
}

/* ── Buzzer ──────────────────────────────────────────────────────────────── */
void buzzer_beep(uint8_t count, uint16_t duration_ms) {
    for (uint8_t i = 0; i < count; i++) {
        nrf_gpio_pin_set(PIN_BUZZER);
        nrf_delay_ms(duration_ms);
        nrf_gpio_pin_clear(PIN_BUZZER);
        if (i < count - 1) nrf_delay_ms(duration_ms);
    }
}

/* ── Transmit to Hub ──────────────────────────────────────────────────────── */
void transmit_to_hub(void) {
    feeding_data_t data = {
        .feeding_state = state.feeding_state,
        .bottle_temp_c_x10 = state.current_temp_c_x10,
        .target_temp_c_x10 = state.target_temp_c_x10,
        .weight_mg = (uint16_t)(state.weight_mg & 0xFFFF),  /* Truncated */
        .start_weight_mg = (uint16_t)(state.start_weight_mg & 0xFFFF),
        .volume_consumed_ml = state.volume_consumed_ml,
        .feeding_duration_s = (uint16_t)(state.feeding_duration_s & 0xFFFF),
        .heater_pct = state.heater_pwm_pct,
        .uv_turbidity = (uint8_t)(state.uv_turbidity >> 4),  /* 12-bit → 8-bit */
        .battery_pct = state.battery_pct,
        .signal_strength = 0,
        .scale_calibrated = state.scale_calibrated,
    };
    
    packet_t pkt = {
        .src = ADDR_FEEDING_STATION,
        .dst = ADDR_HUB,
        .type = PKT_FEEDING_DATA,
        .payload_len = sizeof(data),
    };
    memcpy(pkt.payload, &data, sizeof(data));
    
    radio_send(&pkt);
}

/* ── Scale Calibration ──────────────────────────────────────────────────── */
void calibrate_scale(void) {
    /* Tare: read average with nothing on scale */
    int32_t tare_sum = 0;
    for (int i = 0; i < TARE_SAMPLES; i++) {
        tare_sum += hx711_read(PIN_HX711_DOUT_1, PIN_HX711_SCK_1);
        tare_sum += hx711_read(PIN_HX711_DOUT_2, PIN_HX711_SCK_2);
        nrf_delay_ms(50);
    }
    state.scale_offset = tare_sum / TARE_SAMPLES;
    state.scale_factor = HX711_SCALE_FACTOR;
    state.scale_calibrated = 1;
    state.tare_weight_mg = 0;
}

/* ── Battery Monitoring ──────────────────────────────────────────────────── */
uint8_t read_battery_pct(void) {
    /* Read battery voltage through voltage divider */
    /* nRF52840 SAADC single-ended on AIN5 (P0.29) */
    /* In production: use nrf_drv_saadc */
    /* Voltage divider: VBAT → 1MΩ → PIN → 1MΩ → GND */
    /* Measured voltage = VBAT / 2 */
    float vbat_mv = 3700.0f;  /* Placeholder */
    
    /* LiPo: 4.2V full, 3.0V empty */
    if (vbat_mv > 4200) return 100;
    if (vbat_mv < 3000) return 0;
    return (uint8_t)((vbat_mv - 3000) / 12);  /* Linear: 3.0V=0%, 4.2V=100% */
}

/* ── Main ─────────────────────────────────────────────────────────────────── */
int main(void) {
    /* Initialize nRF52840 */
    nrf_gpio_cfg_input(PIN_HX711_DOUT_1, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_input(PIN_HX711_DOUT_2, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_output(PIN_HX711_SCK_1);
    nrf_gpio_cfg_output(PIN_HX711_SCK_2);
    nrf_gpio_cfg_input(PIN_ONE_WIRE, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_output(PIN_HEATER_PWM);
    nrf_gpio_cfg_output(PIN_UV_LED);
    nrf_gpio_cfg_input(PIN_UV_PHOTODIODE, NRF_GPIO_PIN_PULLDOWN);
    nrf_gpio_cfg_output(PIN_BUZZER);
    nrf_gpio_pin_clear(PIN_BUZZER);
    
    /* Initialize GPIOTE for buttons */
    APP_ERROR_CHECK(nrf_drv_gpiote_init());
    nrf_drv_gpiote_in_config_t btn_config = GPIOTE_CONFIG_IN_SENSE_HITOLO(true);
    btn_config.pull = NRF_GPIO_PIN_PULLUP;
    
    APP_ERROR_CHECK(nrf_drv_gpiote_in_init(PIN_BTN_START, &btn_config, btn_start_handler));
    APP_ERROR_CHECK(nrf_drv_gpiote_in_init(PIN_BTN_DISPENSE, &btn_config, btn_dispense_handler));
    APP_ERROR_CHECK(nrf_drv_gpiote_in_init(PIN_BTN_MODE, &btn_config, btn_mode_handler));
    
    nrf_drv_gpiote_in_event_enable(PIN_BTN_START, true);
    nrf_drv_gpiote_in_event_enable(PIN_BTN_DISPENSE, true);
    nrf_drv_gpiote_in_event_enable(PIN_BTN_MODE, true);
    
    /* Initialize state */
    memset(&state, 0, sizeof(state));
    state.feeding_state = FEED_IDLE;
    state.target_temp_c_x10 = TARGET_TEMP_C_X10;
    state.pid_kp = 5.0f;
    state.pid_ki = 0.5f;
    state.pid_kd = 2.0f;
    state.scale_factor = HX711_SCALE_FACTOR;
    
    /* Power up HX711s */
    hx711_power_up(PIN_HX711_SCK_1);
    hx711_power_up(PIN_HX711_SCK_2);
    
    /* Calibrate scale */
    calibrate_scale();
    
    /* Initialize Sub-GHz radio */
    radio_config_t radio_cfg = {
        .address = ADDR_FEEDING_STATION,
        .frequency = 868000000,
        .spreading_factor = 7,
        .bandwidth = 4,
        .coding_rate = 1,
        .tx_power = 14,
        .preamble_len = 8,
        .sync_word = 0x0C4B,
    };
    radio_init(&radio_cfg);
    
    /* Main loop */
    while (1) {
        uint32_t now = app_timer_cnt_get();
        
        /* Read weight (every 200ms during feeding, every 2s otherwise) */
        uint32_t weight_interval = (state.feeding_state == FEED_IN_PROGRESS) ? 200 : 2000;
        static uint32_t last_weight_time = 0;
        if (now - last_weight_time >= weight_interval) {
            calculate_weight_mg();
            
            /* Update feeding volume */
            if (state.feeding_state == FEED_IN_PROGRESS) {
                state.volume_consumed_ml = calculate_volume_ml(
                    state.start_weight_mg, state.weight_mg);
                state.feeding_duration_s = (now - state.feeding_start_ms) / 1000;
            }
            
            last_weight_time = now;
        }
        
        /* Read temperature (every 500ms during warming, every 5s otherwise) */
        uint32_t temp_interval = (state.feeding_state == FEED_WARMING) ? 500 : 5000;
        static uint32_t last_temp_time = 0;
        if (now - last_temp_time >= temp_interval) {
            int16_t temp_c_x10 = ds18b20_read();
            if (temp_c_x10 > -1000) {  /* Valid reading */
                state.current_temp_c_x10 = temp_c_x10;
            }
            last_temp_time = now;
        }
        
        /* PID heater control during warming */
        if (state.feeding_state == FEED_WARMING) {
            float heater_power = pid_compute(state.target_temp_c_x10, state.current_temp_c_x10);
            heater_set_power((uint8_t)heater_power);
            
            /* Check if target reached */
            if (abs(state.target_temp_c_x10 - state.current_temp_c_x10) < TEMP_TOLERANCE_C_X10) {
                /* Temperature reached — transition to ready */
                state.feeding_state = FEED_READY;
                heater_set_power(0);
                buzzer_beep(2, 200);  /* Two beeps: bottle is ready */
            }
            
            /* Warming timeout */
            if ((now - state.warming_start_ms) / 1000 > WARMING_TIMEOUT_S) {
                state.feeding_state = FEED_IDLE;
                heater_set_power(0);
                buzzer_beep(5, 100);  /* Five short beeps: timeout */
            }
        }
        
        /* Feeding timeout */
        if (state.feeding_state == FEED_IN_PROGRESS) {
            if ((now - state.feeding_start_ms) / 1000 > FEEDING_TIMEOUT_S) {
                state.feeding_state = FEED_DONE;
                state.feeding_duration_s = FEEDING_TIMEOUT_S;
                state.volume_consumed_ml = calculate_volume_ml(
                    state.start_weight_mg, state.weight_mg);
                buzzer_beep(3, 200);  /* Three beeps: feeding timeout */
            }
        }
        
        /* Update OLED display */
        update_oled();
        
        /* Transmit to hub */
        static uint32_t last_tx_time = 0;
        if (now - last_tx_time >= 2000) {
            state.battery_pct = read_battery_pct();
            transmit_to_hub();
            last_tx_time = now;
        }
        
        /* Process received commands from hub */
        packet_t rx_pkt;
        if (radio_receive(&rx_pkt, 0) == 0) {
            if (rx_pkt.dst == ADDR_FEEDING_STATION || rx_pkt.dst == ADDR_BROADCAST) {
                if (rx_pkt.type == PKT_COMMAND && rx_pkt.payload_len >= sizeof(command_payload_t)) {
                    command_payload_t cmd;
                    memcpy(&cmd, rx_pkt.payload, sizeof(cmd));
                    
                    switch (cmd.cmd_id) {
                        case CMD_START_WARMING:
                            state.feeding_state = FEED_WARMING;
                            state.warming_start_ms = now;
                            state.target_temp_c_x10 = TARGET_TEMP_C_X10;
                            break;
                        case CMD_STOP_WARMING:
                            state.feeding_state = FEED_IDLE;
                            heater_set_power(0);
                            break;
                        case CMD_DISPENSE_FORMULA:
                            /* Trigger servo */
                            btn_dispense_handler(0, 0);
                            break;
                        case CMD_PLAY_SOUND:
                            /* Forward to hub (feeding station doesn't play sound) */
                            break;
                        case CMD_SET_TEMP_TARGET:
                            state.target_temp_c_x10 = cmd.param1;
                            break;
                        case CMD_CALIBRATE_SCALE:
                            calibrate_scale();
                            break;
                        default:
                            break;
                    }
                }
            }
        }
        
        /* Low power delay */
        nrf_delay_ms(10);
    }
}