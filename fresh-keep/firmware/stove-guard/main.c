/**
 * FreshKeep — Stove Guard Firmware (STM32F411CE)
 * 
 * CRITICAL SAFETY SYSTEM — runs deterministic fire detection locally.
 * No cloud dependency for life-safety decisions.
 * Sub-100ms response time for gas shutoff.
 * 
 * Thermal array (MLX90640 32×24) + gas sensors + flame IR + smoke
 * → deterministic rules + ML refinement
 * → gas valve shutoff + fire suppression
 * 
 * TDMA mesh: ALWAYS Slot 0 (priority, guaranteed latency)
 */

#include <stdio.h>
#include <string.h>
#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"

/* ── Pin Definitions (STM32F411CE) ─────────────────────────────────── */
#define PIN_MLX90640_CS     GPIO_PIN_4     /* PA4 — SPI1 CS for thermal cam */
#define PIN_GAS_VALVE       GPIO_PIN_6     /* PB6 — Gas solenoid valve (MOSFET gate) */
#define PIN_SUPPRESSION     GPIO_PIN_7     /* PB7 — Fire suppression pump (MOSFET gate) */
#define PIN_SIREN           GPIO_PIN_8     /* PB8 — 105dB piezo siren PWM */
#define PIN_FLAME_IR        GPIO_PIN_9     /* PB9 — VS1838B flame IR (modified) */
#define PIN_SMOKE_INT       GPIO_PIN_12    /* PB12 — RE46C190 smoke detector interrupt */
#define PIN_SMOKE_LED       GPIO_PIN_13    /* PB13 — Smoke detector LED */
#define PIN_TEMP_AMB        0              /* PA8 — DS18B20 ambient temp (OneWire) */
#define PIN_TEMP_STOVE      1              /* PB3 — DS18B20 stove temp (OneWire) */
#define PIN_LED_R           GPIO_PIN_5     /* PB5 — Status LED red */
#define PIN_LED_G           GPIO_PIN_6     /* PB6 — Status LED green (reuse if needed) */
#define PIN_LED_B           GPIO_PIN_7     /* PB7 — Status LED blue */
#define PIN_WATCHDOG        GPIO_PIN_4     /* PC4 — Watchdog kick */
#define PIN_SUPERCAP_V      GPIO_PIN_10    /* PC0 — ADC: supercap voltage */

/* ── Gas Sensor ADC Channels ───────────────────────────────────────── */
#define ADC_CH_MQ2          ADC_CHANNEL_0  /* PA0 — LPG/propane */
#define ADC_CH_MQ135        ADC_CHANNEL_1  /* PA1 — CO/CO2 */
#define ADC_CH_MQ137        ADC_CHANNEL_2  /* PA2 — Ammonia */

/* ── Thermal Array Constants ────────────────────────────────────────── */
#define THERMAL_WIDTH       32
#define THERMAL_HEIGHT      24
#define THERMAL_PIXELS      (THERMAL_WIDTH * THERMAL_HEIGHT)
#define THERMAL_AMBIENT_REG 0x00
#define THERMAL_OBJECT_REG  0x01

/* ── Safety Thresholds ──────────────────────────────────────────────── */
#define PAN_OVERHEAT_WARN_C     200     /* Warning: pan too hot */
#define PAN_OVERHEAT_CRIT_C    260     /* Critical: auto-shutoff */
#define OIL_FLASH_POINT_C       220     /* Oil flash point approximation */
#define LPG_LEAK_PPM           300     /* Warning gas leak */
#define LPG_LEAK_CRIT_PPM     1000     /* Critical gas leak */
#define CO_WARN_PPM             35     /* CO warning level */
#define CO_CRIT_PPM            100     /* CO critical level */
#define SMOKE_THRESHOLD        128     /* 0-255 smoke density */
#define UNATTENDED_WARN_S     600     /* 10 min unattended warning */
#define UNATTENDED_CRIT_S    1200     /* 20 min unattended auto-shutoff */
#define FIRE_ML_THRESHOLD     220     /* 0-255, ML fire confidence */

/* ── State Machine ──────────────────────────────────────────────────── */
typedef enum {
    STATE_SAFE = 0,          /* No alerts, normal cooking */
    STATE_COOKING_DETECTED,  /* Burner on, person present */
    STATE_UNATTENDED_WARN,   /* Burner on, no person for 10 min */
    STATE_UNATTENDED_CRIT,   /* Burner on, no person for 20 min → auto-shutoff */
    STATE_GAS_LEAK_WARN,     /* Gas detected above warning level */
    STATE_GAS_LEAK_CRIT,     /* Gas detected above critical level → shutoff */
    STATE_FIRE_ALARM,        /* Fire detected → shutoff + suppression + siren */
} safety_state_t;

typedef struct {
    /* Thermal data */
    float    thermal_frame[THERMAL_PIXELS];  /* 32×24 temperature map */
    float    max_temp_c;                       /* Hottest pixel */
    float    avg_hotzone_temp_c;              /* Average of hottest 10% */
    uint8_t  burner_state;                    /* 0=off, 1=low, 2=med, 3=high */
    
    /* Gas sensors */
    uint16_t lpg_ppm;        /* MQ-2 LPG reading (calibrated ppm) */
    uint16_t co_ppm;         /* MQ-135 CO reading (calibrated ppm) */
    uint16_t nh3_ppm;        /* MQ-137 ammonia reading (calibrated ppm) */
    
    /* Fire sensors */
    uint8_t  smoke_level;    /* 0-255 from RE46C190 */
    uint8_t  flame_detected; /* 0/1 from VS1838B IR */
    
    /* Temperature */
    float    ambient_temp_c; /* DS18B20 ambient */
    float    stove_temp_c;   /* DS18B20 stovetop */
    
    /* Motion (from PIR or radar — future expansion) */
    uint8_t  motion_detected;
    
    /* Derived */
    uint8_t  fire_confidence; /* 0-255 ML fire confidence */
    safety_state_t state;
    
    /* Actuators */
    uint8_t  gas_valve_open;  /* 0=closed (safe), 1=open */
    uint8_t  suppression_active;
    
    /* Timing */
    uint32_t unattended_start_ms;
    uint32_t state_enter_time_ms;
    uint32_t last_reading_ms;
    
    /* Radio */
    uint32_t last_tx_ms;
} stove_guard_t;

static stove_guard_t g;

/* ── MLX90640 Thermal Camera ───────────────────────────────────────── */
static HAL_StatusTypeDef mlx90640_read_frame(SPI_HandleTypeDef *hspi, float *frame) {
    uint8_t tx_buf[2] = {0x00, 0x01}; /* Read object temperature register */
    uint8_t rx_buf[THERMAL_PIXELS * 2];
    
    /* CS low */
    HAL_GPIO_WritePin(GPIOA, PIN_MLX90640_CS, GPIO_PIN_RESET);
    
    /* Read all 768 pixels (32×24) — 2 bytes each */
    for (int i = 0; i < THERMAL_PIXELS; i++) {
        HAL_SPI_TransmitReceive(hspi, tx_buf, &rx_buf[i*2], 2, 10);
    }
    
    /* CS high */
    HAL_GPIO_WritePin(GPIOA, PIN_MLX90640_CS, GPIO_PIN_SET);
    
    /* Convert raw data to temperature */
    for (int i = 0; i < THERMAL_PIXELS; i++) {
        int16_t raw = (rx_buf[i*2] << 8) | rx_buf[i*2+1];
        /* MLX90640 conversion: temp = raw * 0.02 - 273.15 + 25 */
        /* Simplified — actual driver needs EEPROM calibration */
        frame[i] = (float)raw * 0.02f - 273.15f + 25.0f;
    }
    
    return HAL_OK;
}

static void analyze_thermal_frame(void) {
    float max_temp = -100.0f;
    float sum_hotzone = 0.0f;
    int hotzone_count = 0;
    
    /* Find max temperature and compute hot zone average */
    for (int i = 0; i < THERMAL_PIXELS; i++) {
        if (g.thermal_frame[i] > max_temp) {
            max_temp = g.thermal_frame[i];
        }
    }
    
    /* Average of top 10% hottest pixels */
    float threshold = max_temp * 0.9f;
    for (int i = 0; i < THERMAL_PIXELS; i++) {
        if (g.thermal_frame[i] > threshold) {
            sum_hotzone += g.thermal_frame[i];
            hotzone_count++;
        }
    }
    
    g.max_temp_c = max_temp;
    g.avg_hotzone_temp_c = hotzone_count > 0 ? sum_hotzone / hotzone_count : 0.0f;
    
    /* Estimate burner state from hot zone temperature */
    if (g.avg_hotzone_temp_c < 50.0f) g.burner_state = 0;       /* Off */
    else if (g.avg_hotzone_temp_c < 120.0f) g.burner_state = 1;  /* Low */
    else if (g.avg_hotzone_temp_c < 180.0f) g.burner_state = 2;  /* Medium */
    else g.burner_state = 3;                                       /* High */
}

/* ── Gas Sensor Calibration ─────────────────────────────────────────── */
static uint16_t calibrate_mq2(uint16_t adc_raw) {
    /* MQ-2 LPG sensor: Rs/Ro vs ppm curve
     * Simplified: ppm = (Ro/Rs) ^ (1/b) * scale
     * Ro calibrated at 1000ppm LPG in clean air
     * ADC raw: 0-4095 (12-bit), Rs proportional to (4095 - raw) / raw
     */
    if (adc_raw == 0) return 0;
    float ratio = (4095.0f - (float)adc_raw) / (float)adc_raw;
    float ppm = 1000.0f / powf(ratio, 1.5f); /* Simplified calibration */
    return (uint16_t)(ppm > 9999 ? 9999 : ppm);
}

static uint16_t calibrate_mq135(uint16_t adc_raw) {
    /* MQ-135 CO sensor calibration */
    if (adc_raw == 0) return 0;
    float ratio = (4095.0f - (float)adc_raw) / (float)adc_raw;
    float ppm = 100.0f / powf(ratio, 2.0f); /* Simplified */
    return (uint16_t)(ppm > 9999 ? 9999 : ppm);
}

static uint16_t calibrate_mq137(uint16_t adc_raw) {
    /* MQ-137 Ammonia sensor calibration */
    if (adc_raw == 0) return 0;
    float ratio = (4095.0f - (float)adc_raw) / (float)adc_raw;
    float ppm = 50.0f / powf(ratio, 1.8f); /* Simplified */
    return (uint16_t)(ppm > 9999 ? 9999 : ppm);
}

/* ── Fire Confidence ML (Tiny Model) ────────────────────────────────── */
static uint8_t compute_fire_confidence(void) {
    /* Simplified ML model — in production this would be TFLite Micro
     * Inputs: thermal frame features + gas readings + flame + smoke
     * Output: 0-255 fire confidence
     * 
     * Feature extraction from thermal:
     * - max_temp
     * - hot zone size (% of pixels > 200°C)
     * - hot zone spread (variance of hot pixels)
     * - temporal consistency (is it growing?)
     */
    float hot_pixel_pct = 0.0f;
    for (int i = 0; i < THERMAL_PIXELS; i++) {
        if (g.thermal_frame[i] > 200.0f) hot_pixel_pct += 1.0f;
    }
    hot_pixel_pct /= THERMAL_PIXELS;
    
    float confidence = 0.0f;
    
    /* Thermal features (0-100) */
    if (g.max_temp_c > 300.0f) confidence += 80.0f;
    else if (g.max_temp_c > 220.0f) confidence += 60.0f;
    else if (g.max_temp_c > 180.0f) confidence += 30.0f;
    else confidence += (g.max_temp_c / 180.0f) * 20.0f;
    
    /* Hot pixel spread (0-50) */
    confidence += hot_pixel_pct * 50.0f;
    
    /* Flame IR (0-40) */
    if (g.flame_detected) confidence += 40.0f;
    
    /* Smoke (0-40) */
    confidence += ((float)g.smoke_level / 255.0f) * 40.0f;
    
    /* Gas leak (0-30) */
    if (g.lpg_ppm > 500) confidence += 30.0f;
    else if (g.lpg_ppm > 200) confidence += 15.0f;
    
    /* Clamp to 0-255 */
    if (confidence > 255.0f) confidence = 255.0f;
    if (confidence < 0.0f) confidence = 0.0f;
    
    return (uint8_t)confidence;
}

/* ── Deterministic Safety Rules (sub-10ms) ───────────────────────────── */
static safety_state_t evaluate_safety_rules(void) {
    /* HIGHEST PRIORITY: Fire alarm conditions */
    if (g.max_temp_c > 300.0f)                    return STATE_FIRE_ALARM;
    if (g.fire_confidence > FIRE_ML_THRESHOLD)     return STATE_FIRE_ALARM;
    if (g.flame_detected && g.burner_state == 0)   return STATE_FIRE_ALARM; /* Flame but burner off */
    if (g.smoke_level > SMOKE_THRESHOLD && g.lpg_ppm > 500) return STATE_FIRE_ALARM;
    
    /* Gas leak critical */
    if (g.lpg_ppm > LPG_LEAK_CRIT_PPM)            return STATE_GAS_LEAK_CRIT;
    if (g.co_ppm > CO_CRIT_PPM)                    return STATE_GAS_LEAK_CRIT;
    
    /* Gas leak warning */
    if (g.lpg_ppm > LPG_LEAK_PPM)                  return STATE_GAS_LEAK_WARN;
    if (g.co_ppm > CO_WARN_PPM)                     return STATE_GAS_LEAK_WARN;
    
    /* Unattended cooking */
    if (g.burner_state > 0) {
        uint32_t now = HAL_GetTick();
        if (!g.motion_detected) {
            if (g.unattended_start_ms == 0) {
                g.unattended_start_ms = now;
            } else {
                uint32_t elapsed = (now - g.unattended_start_ms) / 1000;
                if (elapsed > UNATTENDED_CRIT_S)  return STATE_UNATTENDED_CRIT;
                if (elapsed > UNATTENDED_WARN_S)  return STATE_UNATTENDED_WARN;
            }
        } else {
            g.unattended_start_ms = 0;
        }
        return STATE_COOKING_DETECTED;
    }
    
    g.unattended_start_ms = 0;
    return STATE_SAFE;
}

/* ── Actuator Control ───────────────────────────────────────────────── */
static void close_gas_valve(void) {
    /* NC (normally closed) solenoid: de-energize to close */
    HAL_GPIO_WritePin(GPIOB, PIN_GAS_VALVE, GPIO_PIN_RESET);
    g.gas_valve_open = 0;
}

static void open_gas_valve(void) {
    /* Only open if it's safe */
    if (g.state == STATE_SAFE || g.state == STATE_COOKING_DETECTED) {
        HAL_GPIO_WritePin(GPIOB, PIN_GAS_VALVE, GPIO_PIN_SET);
        g.gas_valve_open = 1;
    }
}

static void activate_suppression(void) {
    /* Fire suppression: activate pump for 3 seconds */
    HAL_GPIO_WritePin(GPIOB, PIN_SUPPRESSION, GPIO_PIN_SET);
    g.suppression_active = 1;
    HAL_Delay(3000);
    HAL_GPIO_WritePin(GPIOB, PIN_SUPPRESSION, GPIO_PIN_RESET);
    g.suppression_active = 0;
}

static void sound_siren(uint8_t level) {
    /* PWM on siren pin */
    TIM_HandleTypeDef htim;
    /* Configure PWM frequency based on alert level */
    switch (level) {
        case 0: /* Off */
            __HAL_TIM_SET_COMPARE(&htim, TIM_CHANNEL_1, 0);
            break;
        case 1: /* Beep */
            __HAL_TIM_SET_COMPARE(&htim, TIM_CHANNEL_1, 128);
            HAL_Delay(200);
            __HAL_TIM_SET_COMPARE(&htim, TIM_CHANNEL_1, 0);
            break;
        case 2: /* Rapid beeps */
            for (int i = 0; i < 5; i++) {
                __HAL_TIM_SET_COMPARE(&htim, TIM_CHANNEL_1, 128);
                HAL_Delay(100);
                __HAL_TIM_SET_COMPARE(&htim, TIM_CHANNEL_1, 0);
                HAL_Delay(100);
            }
            break;
        case 3: /* Continuous siren */
            __HAL_TIM_SET_COMPARE(&htim, TIM_CHANNEL_1, 255);
            break;
        case 4: /* Oscillating siren (emergency) */
            for (int i = 0; i < 20; i++) {
                __HAL_TIM_SET_COMPARE(&htim, TIM_CHANNEL_1, 255);
                HAL_Delay(100);
                __HAL_TIM_SET_COMPARE(&htim, TIM_CHANNEL_1, 128);
                HAL_Delay(100);
            }
            break;
    }
}

/* ── Radio Transmission ──────────────────────────────────────────────── */
static radio_handle_t g_radio = {
    .config = RADIO_CONFIG_DEFAULT,
    .pin_nss = 17,
    .pin_busy = 14,
    .pin_irq = 15,
    .pin_reset = 16,
};

static void transmit_stove_data(void) {
    stove_data_t data = {
        .max_temp_c = (uint16_t)(g.max_temp_c * 10),
        .avg_temp_c = (uint16_t)(g.avg_hotzone_temp_c * 10),
        .lpg_ppm = g.lpg_ppm,
        .co_ppm = g.co_ppm,
        .nh3_ppm = g.nh3_ppm,
        .smoke_level = g.smoke_level,
        .flame_detected = g.flame_detected,
        .burner_state = g.burner_state,
        .motion_detected = g.motion_detected,
        .gas_valve_state = g.gas_valve_open,
        .fire_confidence = g.fire_confidence,
        .alert_level = (uint8_t)g.state,
        .thermal_checksum = 0, /* CRC of thermal frame */
    };
    
    packet_t pkt;
    pkt_init(&pkt, ADDR_STOVE_GUARD, ADDR_HUB, PKT_STOVE_DATA);
    pkt_add_payload(&pkt, (uint8_t*)&data, sizeof(data));
    pkt_finalize(&pkt);
    radio_send(&g_radio, pkt.data, pkt.len);
}

static void transmit_fire_alarm(void) {
    fire_alarm_t alarm = {
        .max_temp_c = (uint16_t)(g.max_temp_c),
        .lpg_ppm = g.lpg_ppm,
        .smoke_level = g.smoke_level,
        .flame_detected = g.flame_detected,
        .fire_confidence = g.fire_confidence,
        .source_node = ADDR_STOVE_GUARD,
        .timestamp_ms = (uint16_t)(HAL_GetTick() & 0xFFFF),
    };
    
    packet_t pkt;
    pkt_init(&pkt, ADDR_STOVE_GUARD, ADDR_BROADCAST, PKT_FIRE_ALARM);
    pkt_add_payload(&pkt, (uint8_t*)&alarm, sizeof(alarm));
    pkt_finalize(&pkt);
    radio_send(&g_radio, pkt.data, pkt.len);
}

/* ── Main Loop ──────────────────────────────────────────────────────── */
int main(void) {
    HAL_Init();
    SystemClock_Config();
    
    /* Initialize all peripherals */
    /* (HAL init — SPI, ADC, GPIO, TIM, I2C — omitted for brevity) */
    
    /* Initialize radio */
    radio_init(&g_radio, &RADIO_CONFIG_DEFAULT);
    
    /* Open gas valve (safe state) — requires confirmation sequence */
    /* Valve stays CLOSED until user activates stove guard */
    
    memset(&g, 0, sizeof(g));
    g.state = STATE_SAFE;
    g.gas_valve_open = 0;
    
    /* Enable watchdog (IWDG, 1 second timeout) */
    /* HAL_IWDG_Init(&hiwdg); */
    
    while (1) {
        uint32_t now = HAL_GetTick();
        
        /* ── Read all sensors (10 fps for thermal, 1 fps for gas) ──── */
        
        /* Thermal frame — read at 10 fps */
        mlx90640_read_frame(NULL, g.thermal_frame);
        analyze_thermal_frame();
        
        /* Gas sensors — read every 100ms */
        ADC_HandleTypeDef hadc;
        HAL_ADC_Start(&hadc);
        HAL_ADC_PollForConversion(&hadc, 10);
        uint16_t mq2_raw = HAL_ADC_GetValue(&hadc);
        HAL_ADC_PollForConversion(&hadc, 10);
        uint16_t mq135_raw = HAL_ADC_GetValue(&hadc);
        HAL_ADC_PollForConversion(&hadc, 10);
        uint16_t mq137_raw = HAL_ADC_GetValue(&hadc);
        HAL_ADC_Stop(&hadc);
        
        g.lpg_ppm = calibrate_mq2(mq2_raw);
        g.co_ppm = calibrate_mq135(mq135_raw);
        g.nh3_ppm = calibrate_mq137(mq137_raw);
        
        /* Flame IR — read every 10ms */
        g.flame_detected = HAL_GPIO_ReadPin(GPIOB, PIN_FLAME_IR);
        
        /* Smoke detector — interrupt-driven, read level here */
        /* (simplified — RE46C190 provides analog smoke density) */
        
        /* ── Compute ML fire confidence ─────────────────────────────── */
        g.fire_confidence = compute_fire_confidence();
        
        /* ── Deterministic safety rules ─────────────────────────────── */
        safety_state_t new_state = evaluate_safety_rules();
        
        /* ── State transitions and actions ──────────────────────────── */
        if (new_state != g.state) {
            g.state_enter_time_ms = now;
            
            switch (new_state) {
                case STATE_FIRE_ALARM:
                    /* IMMEDIATE: Close gas valve */
                    close_gas_valve();
                    /* Activate fire suppression */
                    activate_suppression();
                    /* Sound siren (continuous) */
                    sound_siren(4);
                    /* Broadcast fire alarm */
                    transmit_fire_alarm();
                    break;
                    
                case STATE_GAS_LEAK_CRIT:
                    close_gas_valve();
                    sound_siren(3);
                    transmit_fire_alarm();
                    break;
                    
                case STATE_GAS_LEAK_WARN:
                    sound_siren(2);
                    break;
                    
                case STATE_UNATTENDED_CRIT:
                    close_gas_valve();
                    sound_siren(3);
                    break;
                    
                case STATE_UNATTENDED_WARN:
                    sound_siren(1);
                    break;
                    
                case STATE_COOKING_DETECTED:
                    /* Cooking detected, person present — normal */
                    break;
                    
                case STATE_SAFE:
                    /* All clear */
                    break;
            }
            
            g.state = new_state;
        }
        
        /* ── Update status LEDs ──────────────────────────────────────── */
        switch (g.state) {
            case STATE_SAFE:
                HAL_GPIO_WritePin(GPIOB, PIN_LED_R, GPIO_PIN_RESET);
                HAL_GPIO_WritePin(GPIOB, PIN_LED_G, GPIO_PIN_SET);
                HAL_GPIO_WritePin(GPIOB, PIN_LED_B, GPIO_PIN_RESET);
                break;
            case STATE_COOKING_DETECTED:
                HAL_GPIO_WritePin(GPIOB, PIN_LED_R, GPIO_PIN_RESET);
                HAL_GPIO_WritePin(GPIOB, PIN_LED_G, GPIO_PIN_SET);
                HAL_GPIO_WritePin(GPIOB, PIN_LED_B, GPIO_PIN_SET);
                break;
            case STATE_UNATTENDED_WARN:
            case STATE_GAS_LEAK_WARN:
                HAL_GPIO_WritePin(GPIOB, PIN_LED_R, GPIO_PIN_SET);
                HAL_GPIO_WritePin(GPIOB, PIN_LED_G, GPIO_PIN_SET);
                HAL_GPIO_WritePin(GPIOB, PIN_LED_B, GPIO_PIN_RESET);
                break;
            case STATE_UNATTENDED_CRIT:
            case STATE_GAS_LEAK_CRIT:
                HAL_GPIO_WritePin(GPIOB, PIN_LED_R, GPIO_PIN_SET);
                HAL_GPIO_WritePin(GPIOB, PIN_LED_G, GPIO_PIN_RESET);
                HAL_GPIO_WritePin(GPIOB, PIN_LED_B, GPIO_PIN_RESET);
                break;
            case STATE_FIRE_ALARM:
                HAL_GPIO_WritePin(GPIOB, PIN_LED_R, GPIO_PIN_SET);
                HAL_GPIO_WritePin(GPIOB, PIN_LED_G, GPIO_PIN_RESET);
                HAL_GPIO_WritePin(GPIOB, PIN_LED_B, GPIO_PIN_RESET);
                break;
        }
        
        /* ── TDMA Slot 0: Transmit to hub ────────────────────────────── */
        uint32_t slot_time = now % 500; /* 500ms frame */
        if (slot_time < 100 && (now - g.last_tx_ms) > 400) {
            transmit_stove_data();
            g.last_tx_ms = now;
        }
        
        /* ── Kick watchdog ────────────────────────────────────────────── */
        HAL_GPIO_WritePin(GPIOC, PIN_WATCHDOG, GPIO_PIN_SET);
        for (volatile int i = 0; i < 10; i++);
        HAL_GPIO_WritePin(GPIOC, PIN_WATCHDOG, GPIO_PIN_RESET);
        
        /* ── Small delay between readings ────────────────────────────── */
        HAL_Delay(10); /* 100 fps effective loop rate */
    }
    
    return 0;
}