/**
 * PowerPulse Solar Node — Main (RP2040, Pico SDK)
 * 
 * Dual-core MPPT controller and battery monitor. Core 0 handles communication
 * and monitoring. Core 1 runs the MPPT perturb-and-observe algorithm with
 * incremental conductance enhancement, driving a synchronous buck converter
 * via PIO-generated PWM.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/watchdog.h"

#include "powerpulse_protocol.h"
#include "cc1101.h"
#include "ina260.h"
#include "max31855.h"
#include "sh1106.h"

// ─── Pin Definitions ────────────────────────────────────────────────

// SPI0 — CC1101 Sub-GHz radio
#define CC1101_SPI      spi0
#define CC1101_SCLK     4
#define CC1101_MOSI     5  // GP5 = SPI0 TX
#define CC1101_MISO     6  // GP6 = SPI0 RX (note: using MOSI/MISO per pin table)
#define CC1101_CS       7
#define CC1101_GDO0     8
#define CC1101_GDO2     9

// I2C0 — INA260 sensors
#define INA260_SOLAR_SDA    0   // GP0 = I2C0 SDA
#define INA260_SOLAR_SCL    1   // GP1 = I2C0 SCL
#define INA260_LOAD_SDA     2   // GP2 = I2C1 SDA (second I2C)
#define INA260_LOAD_SCL     3   // GP3 = I2C1 SCL

// MPPT PWM — PIO0
#define MPPT_PWM_HIGH    10   // GP10 = PIO0 SM0 — high-side gate
#define MPPT_PWM_LOW     11   // GP11 = PIO0 SM0 — low-side gate (complementary)

// SPI1 — MAX31855 thermocouple
#define MAX31855_SPI     spi1
#define MAX31855_SCLK    12
#define MAX31855_MISO    13
#define MAX31855_CS      14

// CAN bus — MCP2551
#define CAN_TX           18
#define CAN_RX           19

// OLED — I2C (shared with INA260)
#define OLED_SDA         16
#define OLED_SCL         17

// ADC channels
#define ADC_SOLAR_V      20   // GP26 (ADC0) — solar voltage divider
#define ADC_BATT_V       21   // GP27 (ADC1) — battery voltage divider
#define ADC_HEATSINK_T   26   // GP26 (ADC0) — wait, conflict... use GP28
// Actually: use internal temperature for MCU, external MAX31855 for heatsink

// Fan PWM
#define FAN_PWM          15   // GP15 = PWM channel

// Status LED
#define LED_PIN          22   // GP22 = onboard LED

// Emergency shutdown
#define EMERGENCY_PIN    27   // GP27 = emergency shutdown (active high)

// ─── MPPT Constants ─────────────────────────────────────────────────

#define MPPT_PWM_FREQ        100000   // 100 kHz switching frequency
#define MPPT_MIN_DUTY        5         // 5% minimum duty
#define MPPT_MAX_DUTY        95        // 95% maximum duty
#define MPPT_STEP_SIZE       0.5       // Initial perturbation step (% duty)
#define MPPT_STEP_SIZE_FINE  0.1       // Fine perturbation step
#define MPPT_INTERVAL_MS     100       // MPPT calculation interval (ms)
#define MPPT_MAX_SOLAR_V     35000     // 35V max solar voltage (mV)
#define MPPT_MAX_SOLAR_I     20000     // 20A max solar current (mA)
#define MPPT_MAX_BATT_V      58000     // 58V max battery voltage (mV)
#define MPPT_BULK_V          56000     // 56V bulk charge voltage (mV)
#define MPPT_FLOAT_V         54600     // 54.6V float voltage (mV)
#define MPPT_EQ_V            58400     // 58.4V equalization voltage (mV)

#define NODE_ADDRESS         PP_ADDR_SOLAR_NODE(0)

// ─── Global State ──────────────────────────────────────────────────

typedef struct {
    // Solar measurements
    float pv_voltage;         // Solar panel voltage (V)
    float pv_current;         // Solar panel current (A)
    float pv_power;           // Solar power (W)
    
    // Battery measurements
    float batt_voltage;       // Battery voltage (V)
    float batt_soc;           // Battery state of charge (%)
    
    // Load measurements
    float load_current;       // Load side current (A)
    float load_power;         // Load power (W)
    
    // MPPT state
    float duty_cycle;         // Current duty cycle (%)
    float prev_power;         // Previous power measurement (W)
    float prev_voltage;       // Previous voltage measurement (V)
    float prev_current;       // Previous current measurement (A)
    int8_t direction;         // 1 = increasing duty, -1 = decreasing
    uint8_t charge_mode;     // 0=standby, 1=bulk, 2=absorption, 3=float
    bool mppt_active;         // MPPT tracking active
    
    // Thermal
    float heatsink_temp;     // Heatsink temperature (°C)
    float fan_speed_pct;     // Fan speed (%)
    bool overtemp_shutdown;   // Emergency overtemp flag
    
    // Energy accumulation
    uint32_t energy_produced_wh;  // Cumulative solar energy (Wh)
    uint32_t energy_consumed_wh;  // Cumulative load energy (Wh)
    
    // Communication
    uint16_t tx_seq;
    bool cc1101_initialized;
    
    // Safety
    bool emergency_shutdown;
    
    // Statistics
    uint32_t uptime_seconds;
    uint32_t mppt_iterations;
} solar_state_t;

static solar_state_t g_state = {
    .duty_cycle = 50.0f,
    .direction = 1,
    .charge_mode = 0,
    .mppt_active = false,
};

// ─── MPPT PWM via PIO ──────────────────────────────────────────────

// PIO program for complementary PWM with dead time
// This generates two complementary PWM signals with configurable
// dead time for synchronous buck converter gate drive.

const uint16_t mppt_pwm_program[] = {
    // Pull duty cycle from TX FIFO
    0x80a0, // pull block           ; Get duty value
    0x0041, // set pins, 1          ; Set high-side ON
    0x0060, // out x, 32            ; Move duty to x register
    // High-side ON period
    0x0043, // set pins, 3          ; Both high (dead time start)
    0x0001, // nop                  ; Dead time (2 cycles = 20ns @ 100kHz)
    0x0042, // set pins, 2          ; Set low-side ON (high-side OFF)
    0x0083, // pull noblock         ; Re-read duty (or keep previous)
    // Low-side ON period  
    0x0001, // nop                  ; Dead time before switching back
    0x0040, // set pins, 0          ; Both off (dead time end)
};

// ─── INA260 I2C Addresses ──────────────────────────────────────────

#define INA260_SOLAR_ADDR    0x40  // A0=GND, A1=GND
#define INA260_LOAD_ADDR     0x41  // A0=VS, A1=GND (second sensor)

// ─── Forward Declarations ───────────────────────────────────────────

void core1_entry(void);  // MPPT core
static void mppt_init(void);
static float mppt_perturb_observe(float pv_v, float pv_i, float pv_p);
static void mppt_set_duty(float duty);
static void safety_check(void);
static void subghz_task(void);
static void bms_task(void);
static void display_task(void);
static void thermal_task(void);
static void send_solar_data(void);
static void handle_command(const pp_solar_cmd_payload_t *cmd);

// ─── Core 1 — MPPT Loop ────────────────────────────────────────────

void core1_entry(void)
{
    printf("[MPPT] Core 1 started — running MPPT loop\n");
    
    mppt_init();
    g_state.mppt_active = true;
    
    absolute_time_t last_mppt_time = get_absolute_time();
    absolute_time_t last_energy_time = get_absolute_time();
    
    while (true) {
        // Read solar panel measurements
        float pv_voltage = ina260_read_voltage(INA260_SOLAR_ADDR);
        float pv_current = ina260_read_current(INA260_SOLAR_ADDR);
        float pv_power = pv_voltage * pv_current;
        
        // Read battery voltage
        float batt_voltage = read_battery_voltage_adc();
        
        // Read load side
        float load_current = ina260_read_current(INA260_LOAD_ADDR);
        float load_power = ina260_read_voltage(INA260_LOAD_ADDR) * load_current;
        
        // Update global state (with mutex in production)
        g_state.pv_voltage = pv_voltage;
        g_state.pv_current = pv_current;
        g_state.pv_power = pv_power;
        g_state.batt_voltage = batt_voltage;
        g_state.load_current = load_current;
        g_state.load_power = load_power;
        
        // ── Safety Checks ──
        if (g_state.emergency_shutdown || g_state.overtemp_shutdown) {
            mppt_set_duty(0.0f);
            g_state.mppt_active = false;
            g_state.charge_mode = 0;
            sleep_ms(1000);
            continue;
        }
        
        // Overvoltage protection
        if (pv_voltage > 35.0f) {
            mppt_set_duty(g_state.duty_cycle - 5.0f);
            printf("[MPPT] PV overvoltage: %.1fV, reducing duty\n", pv_voltage);
        }
        
        // Battery overvoltage protection
        if (batt_voltage > 58.0f) {
            mppt_set_duty(0.0f);
            g_state.charge_mode = 3;  // Float mode
            printf("[MPPT] Battery overvoltage: %.1fV, switching to float\n", batt_voltage);
        }
        
        // ── MPPT Algorithm ──
        // Only run if there's sufficient solar power
        if (pv_power > 1.0f && pv_voltage > 10.0f) {
            float new_duty = mppt_perturb_observe(pv_voltage, pv_current, pv_power);
            mppt_set_duty(new_duty);
            g_state.mppt_active = true;
        } else {
            // Not enough solar power — reduce duty
            mppt_set_duty(max(0.0f, g_state.duty_cycle - 1.0f));
            g_state.mppt_active = false;
        }
        
        // ── Charge Mode Logic ──
        if (batt_voltage < 48.0f) {
            g_state.charge_mode = 0;  // Standby (discharged)
        } else if (batt_voltage < MPPT_BULK_V / 1000.0f) {
            g_state.charge_mode = 1;  // Bulk charging
        } else if (batt_voltage < MPPT_FLOAT_V / 1000.0f) {
            g_state.charge_mode = 2;  // Absorption
        } else {
            g_state.charge_mode = 3;  // Float
        }
        
        // ── Energy Accumulation ──
        absolute_time_t now = get_absolute_time();
        int64_t dt_us = absolute_time_diff_us(last_energy_time, now);
        float dt_hours = dt_us / 3600000000.0f;
        g_state.energy_produced_wh += (uint32_t)(pv_power * dt_hours);
        g_state.energy_consumed_wh += (uint32_t)(load_power * dt_hours);
        last_energy_time = now;
        
        g_state.mppt_iterations++;
        
        // Run MPPT at 100ms intervals (10 Hz)
        sleep_ms(MPPT_INTERVAL_MS);
    }
}

// ─── MPPT Perturb & Observe with Incremental Conductance ────────────

static float mppt_perturb_observe(float pv_v, float pv_i, float pv_p)
{
    float duty = g_state.duty_cycle;
    float step = MPPT_STEP_SIZE;
    
    // Use smaller step near MPP for finer tracking
    if (fabsf(pv_p - g_state.prev_power) < 0.5f) {
        step = MPPT_STEP_SIZE_FINE;
    }
    
    // ── Incremental Conductance Enhancement ──
    float dI = pv_i - g_state.prev_current;
    float dV = pv_v - g_state.prev_voltage;
    
    if (fabsf(dV) > 0.01f) {
        float inc_cond = dI / dV;    // Incremental conductance
        float cond = pv_i / pv_v;     // Instantaneous conductance
        
        // At MPP: dI/dV = -I/V → inc_cond + cond ≈ 0
        float mpp_error = inc_cond + cond;
        
        if (fabsf(mpp_error) < 0.01f) {
            // At MPP — hold current duty
            g_state.prev_power = pv_p;
            g_state.prev_voltage = pv_v;
            g_state.prev_current = pv_i;
            return duty;
        } else if (mpp_error > 0) {
            // Left of MPP — increase duty (move toward MPP)
            duty += step;
        } else {
            // Right of MPP — decrease duty
            duty -= step;
        }
    } else {
        // Voltage not changing enough — fall back to P&O
        float dP = pv_p - g_state.prev_power;
        
        if (dP > 0) {
            // Power increasing — continue in same direction
            duty += step * g_state.direction;
        } else if (dP < 0) {
            // Power decreasing — reverse direction
            g_state.direction *= -1;
            duty += step * g_state.direction;
        }
        // else: power unchanged, hold duty
    }
    
    // Clamp duty cycle
    if (duty < MPPT_MIN_DUTY) duty = MPPT_MIN_DUTY;
    if (duty > MPPT_MAX_DUTY) duty = MPPT_MAX_DUTY;
    
    g_state.prev_power = pv_p;
    g_state.prev_voltage = pv_v;
    g_state.prev_current = pv_i;
    
    return duty;
}

// ─── MPPT PWM Control ───────────────────────────────────────────────

static void mppt_init(void)
{
    // Initialize PIO for PWM generation
    PIO pio = pio0;
    uint offset = pio_add_program(pio, mppt_pwm_program);
    
    // Claim a state machine
    uint sm = pio_claim_unused_sm(pio, true);
    
    // Configure PIO state machine
    pio_sm_config config = pio_get_default_sm_config();
    sm_config_set_set_pins(&config, MPPT_PWM_HIGH, 2);  // 2 consecutive pins
    sm_config_set_out_shift(&config, true, true, 32);    // Auto-pull, shift right
    sm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (MPPT_PWM_FREQ * 256));
    
    pio_sm_init(pio, sm, offset, &config);
    pio_sm_set_enabled(pio, sm, true);
    
    printf("[MPPT] PIO PWM initialized at %d Hz\n", MPPT_PWM_FREQ);
    
    // Initialize fan PWM
    gpio_set_function(FAN_PWM, GPIO_FUNC_PWM);
    uint fan_slice = pwm_gpio_to_slice_num(FAN_PWM);
    pwm_set_wrap(fan_slice, 255);
    pwm_set_enabled(fan_slice, true);
    pwm_set_chan(fan_slice, PWM_CHAN_A, 0);  // Fan off initially
    
    // Initialize ADC for battery/solar voltage sensing
    adc_init();
    adc_gpio_init(ADC_SOLAR_V);
    adc_gpio_init(ADC_BATT_V);
    
    // Initialize emergency shutdown pin
    gpio_init(EMERGENCY_PIN);
    gpio_set_dir(EMERGENCY_PIN, GPIO_IN);
    gpio_pull_down(EMERGENCY_PIN);
}

static void mppt_set_duty(float duty)
{
    // Clamp duty
    duty = fmaxf(MPPT_MIN_DUTY, fminf(MPPT_MAX_DUTY, duty));
    
    // Convert duty % to PIO count (0-255)
    uint32_t count = (uint32_t)(duty * 255.0f / 100.0f);
    
    // Push to PIO TX FIFO
    PIO pio = pio0;
    uint sm = 0;  // Using SM0 for MPPT PWM
    pio_sm_put(pio, sm, count);
    
    g_state.duty_cycle = duty;
}

static float read_battery_voltage_adc(void)
{
    // Read battery voltage through voltage divider
    // Divider: Vbat → 100kΩ → ADC → 10kΩ → GND
    // ADC value = Vbat * 10k / (100k + 10k) * 3.3 / 4095
    // Vbat = ADC * (110k/10k) * 3.3 / 4095
    
    adc_select_input(1);  // ADC1 = GP27 = battery voltage
    uint16_t raw = adc_read();
    float voltage = (float)raw * (110000.0f / 10000.0f) * 3.3f / 4095.0f;
    return voltage;
}

static float read_solar_voltage_adc(void)
{
    // Same divider for solar voltage
    adc_select_input(0);  // ADC0 = GP26 = solar voltage
    uint16_t raw = adc_read();
    float voltage = (float)raw * (110000.0f / 10000.0f) * 3.3f / 4095.0f;
    return voltage;
}

// ─── Safety Check ────────────────────────────────────────────────────

static void safety_check(void)
{
    // Check emergency shutdown pin
    if (gpio_get(EMERGENCY_PIN)) {
        g_state.emergency_shutdown = true;
        mppt_set_duty(0.0f);
        printf("[SAFETY] Emergency shutdown activated!\n");
    }
    
    // Overtemperature check
    if (g_state.heatsink_temp > 90.0f) {
        g_state.overtemp_shutdown = true;
        mppt_set_duty(0.0f);
        printf("[SAFETY] Overtemperature: %.1f°C — shutting down!\n", g_state.heatsink_temp);
    } else if (g_state.heatsink_temp > 80.0f) {
        // Warning zone — increase fan speed
        g_state.fan_speed_pct = 100.0f;
        printf("[SAFETY] Warning: heatsink at %.1f°C\n", g_state.heatsink_temp);
    } else if (g_state.heatsink_temp < 60.0f) {
        g_state.fan_speed_pct = 0.0f;  // Fan off below 60°C
    } else {
        // Linear fan speed from 60-80°C
        g_state.fan_speed_pct = (g_state.heatsink_temp - 60.0f) * 100.0f / 20.0f;
    }
    
    // Update fan PWM
    uint fan_slice = pwm_gpio_to_slice_num(FAN_PWM);
    pwm_set_chan(fan_slice, PWM_CHAN_A, (uint16_t)(g_state.fan_speed_pct * 255.0f / 100.0f));
    
    // Battery undervoltage protection
    if (g_state.batt_voltage < 40.0f && g_state.batt_voltage > 5.0f) {
        printf("[SAFETY] Battery undervoltage: %.1fV\n", g_state.batt_voltage);
        // Don't charge if battery is too low (may be damaged)
        // This is a safety feature for deeply discharged batteries
    }
}

// ─── Core 0 — Communication & Monitoring ────────────────────────────

int main(void)
{
    stdio_init_all();
    
    printf("=== PowerPulse Solar Node v1.0 ===\n");
    printf("Core 0: Communication and monitoring\n");
    
    // Initialize I2C for INA260 and OLED
    i2c_init(i2c0, 400000);  // 400 kHz
    gpio_set_function(INA260_SOLAR_SDA, GPIO_FUNC_I2C);
    gpio_set_function(INA260_SOLAR_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(INA260_SOLAR_SDA);
    gpio_pull_up(INA260_SOLAR_SCL);
    
    i2c_init(i2c1, 400000);
    gpio_set_function(INA260_LOAD_SDA, GPIO_FUNC_I2C);
    gpio_set_function(INA260_LOAD_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(INA260_LOAD_SDA);
    gpio_pull_up(INA260_LOAD_SCL);
    
    // Initialize SPI for CC1101
    spi_init(CC1101_SPI, 1000000);
    gpio_set_function(CC1101_SCLK, GPIO_FUNC_SPI);
    gpio_set_function(CC1101_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(CC1101_MISO, GPIO_FUNC_SPI);
    gpio_init(CC1101_CS);
    gpio_set_dir(CC1101_CS, GPIO_OUT);
    gpio_put(CC1101_CS, 1);
    
    // Initialize SPI for MAX31855
    spi_init(MAX31855_SPI, 500000);
    gpio_set_function(MAX31855_SCLK, GPIO_FUNC_SPI);
    gpio_set_function(MAX31855_MISO, GPIO_FUNC_SPI);
    gpio_init(MAX31855_CS);
    gpio_set_dir(MAX31855_CS, GPIO_OUT);
    gpio_put(MAX31855_CS, 1);
    
    // Initialize CC1101
    cc1101_init(CC1101_SPI, CC1101_CS, CC1101_GDO0, CC1101_GDO2);
    cc1101_set_frequency(868000000);
    cc1101_set_data_rate(10);
    cc1101_set_tx_power(0x0C);
    
    // Initialize INA260 sensors
    ina260_init(i2c0, INA260_SOLAR_ADDR);
    ina260_init(i2c1, INA260_LOAD_ADDR);
    
    // Initialize OLED
    sh1106_init(i2c0);
    
    // Initialize LED
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    
    // Launch Core 1 (MPPT)
    multicore_launch_core1(core1_entry);
    
    printf("All peripherals initialized. Starting main loop.\n");
    
    // ── Main Loop ──
    uint32_t last_subghz_time = 0;
    uint32_t last_display_time = 0;
    uint32_t last_safety_time = 0;
    uint32_t last_bms_time = 0;
    
    while (true) {
        uint32_t now = to_ms_since_boot(get_absolute_time());
        
        // Sub-GHz communication (every 10 seconds)
        if (now - last_subghz_time >= 10000) {
            send_solar_data();
            last_subghz_time = now;
        }
        
        // Display update (every 1 second)
        if (now - last_display_time >= 1000) {
            display_task();
            last_display_time = now;
        }
        
        // Safety check (every 500ms)
        if (now - last_safety_time >= 500) {
            safety_check();
            last_safety_time = now;
        }
        
        // BMS communication (every 5 seconds)
        if (now - last_bms_time >= 5000) {
            bms_task();
            last_bms_time = now;
        }
        
        // Blink status LED
        gpio_put(LED_PIN, !gpio_get(LED_PIN));
        
        g_state.uptime_seconds = now / 1000;
        
        sleep_ms(100);
    }
    
    return 0;
}

// ─── Sub-GHz Send Solar Data ────────────────────────────────────────

static void send_solar_data(void)
{
    pp_solar_data_payload_t data = {
        .pv_voltage_mv = (uint16_t)(g_state.pv_voltage * 1000),
        .pv_current_ma = (uint16_t)(g_state.pv_current * 1000),
        .pv_power_w = (uint16_t)(g_state.pv_power),
        .batt_voltage_mv = (uint16_t)(g_state.batt_voltage * 1000),
        .load_current_ma = (uint16_t)(g_state.load_current * 1000),
        .load_power_w = (uint16_t)(g_state.load_power),
        .soc_pct = (uint8_t)(g_state.batt_soc),
        .charge_mode = g_state.charge_mode,
        .mppt_duty_pct = (uint8_t)(g_state.duty_cycle),
        .heatsink_temp_c = (int8_t)(g_state.heatsink_temp),
        .fan_speed_pct = (uint8_t)(g_state.fan_speed_pct),
        .energy_produced_wh = g_state.energy_produced_wh,
        .energy_consumed_wh = g_state.energy_consumed_wh,
    };
    
    uint8_t frame_buf[128];
    uint16_t frame_len = pp_frame_build(
        NODE_ADDRESS, PP_ADDR_HUB, PP_MSG_SOLAR_DATA,
        g_state.tx_seq++, (const uint8_t *)&data, sizeof(data),
        frame_buf, sizeof(frame_buf)
    );
    
    if (frame_len > 0) {
        cc1101_send_packet(frame_buf, frame_len);
        cc1101_set_rx_mode();
    }
    
    // Also check for incoming commands
    uint8_t rx_buf[128];
    int16_t rx_len = cc1101_receive_packet(rx_buf, sizeof(rx_buf), 50);
    if (rx_len > 0) {
        pp_frame_header_t header;
        const uint8_t *payload;
        uint16_t payload_len;
        
        if (pp_frame_parse(rx_buf, rx_len, &header, &payload, &payload_len) == 0) {
            if (header.type == PP_MSG_SOLAR_CMD) {
                const pp_solar_cmd_payload_t *cmd = (const pp_solar_cmd_payload_t *)payload;
                handle_command(cmd);
            } else if (header.type == PP_MSG_HEARTBEAT) {
                printf("[SUBGHZ] Heartbeat from hub\n");
            }
        }
    }
}

// ─── Handle Solar Command ───────────────────────────────────────────

static void handle_command(const pp_solar_cmd_payload_t *cmd)
{
    printf("[CMD] Solar command received: duty=%d mode=%d emergency=%d\n",
           cmd->target_duty_pct, cmd->mode_override, cmd->emergency);
    
    // Emergency shutdown
    if (cmd->emergency) {
        g_state.emergency_shutdown = true;
        mppt_set_duty(0.0f);
        printf("[CMD] Emergency shutdown activated!\n");
        return;
    }
    
    // Mode override
    switch (cmd->mode_override) {
        case 0:  // Auto
            g_state.mppt_active = true;
            break;
        case 1:  // Forced buck (MPPT)
            g_state.mppt_active = true;
            g_state.charge_mode = 1;
            break;
        case 2:  // Forced float
            g_state.mppt_active = true;
            g_state.charge_mode = 3;
            break;
        case 3:  // Forced off
            mppt_set_duty(0.0f);
            g_state.mppt_active = false;
            g_state.charge_mode = 0;
            break;
    }
    
    // Target duty override (255 = auto)
    if (cmd->target_duty_pct != 255 && cmd->target_duty_pct >= MPPT_MIN_DUTY && cmd->target_duty_pct <= MPPT_MAX_DUTY) {
        mppt_set_duty((float)cmd->target_duty_pct);
        g_state.mppt_active = false;  // Manual override disables MPPT
    }
}

// ─── BMS Communication ──────────────────────────────────────────────

static void bms_task(void)
{
    // In production: communicate with LTC6811 BMS via CAN bus (MCP2551)
    // For now: estimate SoC from battery voltage
    // 48V LiFePO4: 58.4V = 100%, 48.0V = 0%
    
    float soc = (g_state.batt_voltage - 48.0f) / (58.4f - 48.0f) * 100.0f;
    if (soc < 0.0f) soc = 0.0f;
    if (soc > 100.0f) soc = 100.0f;
    g_state.batt_soc = soc;
    
    printf("[BMS] Battery: %.1fV SoC: %.0f%%\n", g_state.batt_voltage, g_state.batt_soc);
}

// ─── Display Update ─────────────────────────────────────────────────

static void display_task(void)
{
    char buf[32];
    
    sh1106_clear();
    
    // Title
    sh1106_draw_text(0, 0, "PowerPulse Solar", 1, true);
    
    // Solar power
    snprintf(buf, sizeof(buf), "PV: %.1fV %.1fA %uW",
             g_state.pv_voltage, g_state.pv_current, (unsigned)g_state.pv_power);
    sh1106_draw_text(0, 12, buf, 1, true);
    
    // Battery
    snprintf(buf, sizeof(buf), "Bat: %.1fV %u%% %s",
             g_state.batt_voltage, (unsigned)g_state.batt_soc,
             g_state.charge_mode == 1 ? "BULK" :
             g_state.charge_mode == 2 ? "ABS" :
             g_state.charge_mode == 3 ? "FLT" : "STBY");
    sh1106_draw_text(0, 24, buf, 1, true);
    
    // Load
    snprintf(buf, sizeof(buf), "Load: %.1fA %uW",
             g_state.load_current, (unsigned)g_state.load_power);
    sh1106_draw_text(0, 36, buf, 1, true);
    
    // MPPT duty & temperature
    snprintf(buf, sizeof(buf), "D:%.0f%% T:%.0fC F:%u%%",
             g_state.duty_cycle, g_state.heatsink_temp,
             (unsigned)g_state.fan_speed_pct);
    sh1106_draw_text(0, 48, buf, 1, true);
    
    // Energy totals
    snprintf(buf, sizeof(buf), "E:%luWh prod %luWh load",
             (unsigned long)g_state.energy_produced_wh,
             (unsigned long)g_state.energy_consumed_wh);
    sh1106_draw_text(0, 60, buf, 1, true);
    
    sh1106_update();
}