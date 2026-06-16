/**
 * CradleKeep — Crib Pad Node Firmware (STM32L476RG)
 * 
 * Ultra-thin sensor pad that slides under the mattress.
 * Measures breathing (ballistocardiography), movement, position,
 * temperature, and wetness — all without any contact with the baby.
 * 
 * Power: CR2450 coin cell (18+ months at ultra-low duty cycle)
 * Radio: SX1261 Sub-GHz LoRa (mesh with hub)
 * 
 * CRITICAL: This node's breathing data is the highest-priority
 * safety data in the system. It always gets TDMA Slot 0.
 */

#include <stdio.h>
#include <string.h>
#include "stm32l4xx.h"
#include "stm32l4xx_hal.h"

/* ── Pin Definitions (STM32L476RG) ─────────────────────────────────── */
#define PIN_FSR1_ADC       ADC_IN5   /* PA0 — Force sensor 1 (head) */
#define PIN_FSR2_ADC       ADC_IN6   /* PA1 — Force sensor 2 (chest) */
#define PIN_FSR3_ADC       ADC_IN7   /* PA2 — Force sensor 3 (hip left) */
#define PIN_FSR4_ADC       ADC_IN8   /* PA3 — Force sensor 4 (hip right) */
#define PIN_WET1_ADC       ADC_IN14  /* PA4 — Conductive trace 1 */
#define PIN_WET2_ADC       ADC_IN15  /* PA5 — Conductive trace 2 */
#define PIN_VBAT_ADC       ADC_IN17  /* PA6 — Battery voltage monitor */
#define PIN_RADIO_SCK      3         /* PB3 — SX1261 SPI clock */
#define PIN_RADIO_MISO     4         /* PB4 — SX1261 SPI MISO */
#define PIN_RADIO_MOSI     5         /* PB5 — SX1261 SPI MOSI */
#define PIN_RADIO_NSS      6         /* PB6 — SX1261 chip select */
#define PIN_RADIO_BUSY     7         /* PB7 — SX1261 busy */
#define PIN_RADIO_IRQ      8         /* PB8 — SX1261 DIO1 interrupt */
#define PIN_RADIO_RST      9         /* PB9 — SX1261 reset */
#define PIN_I2C_SDA        10        /* PA9 — SHT40 + LIS3DH I2C data */
#define PIN_I2C_SCL        11        /* PA10 — SHT40 + LIS3DH I2C clock */
#define PIN_ACCEL_INT      15        /* PA15 — LIS3DH interrupt */

/* ── Constants ─────────────────────────────────────────────────────── */
#define FSR_SAMPLE_RATE_HZ    200    /* 200 Hz sampling for BCG */
#define BCG_WINDOW_MS          5000   /* 5-second analysis window */
#define BREATH_RATE_MIN        15     /* Below this = bradypnea alert */
#define BREATH_RATE_MAX        70     /* Above this = tachypnea alert */
#define APNEA_THRESHOLD_MS    3000    /* >3s between breaths = apnea event */
#define MOVEMENT_THRESHOLD    20      /* Movement score threshold */
#define WETNESS_THRESHOLD      100    /* Conductivity threshold for wetness */
#define TX_INTERVAL_NORMAL_MS  2000   /* Transmit every 2 seconds */
#define TX_INTERVAL_ALERT_MS   500    /* Transmit every 500ms during alert */
#define BATTERY_LOW_MV         2400   /* CR2450 low battery threshold */

/* ── BCG Processing ─────────────────────────────────────────────────── */
/* Bandpass filter for breathing signal: 0.2-2.0 Hz (12-120 BPM) */
/* Implemented as biquad IIR filter */
typedef struct {
    float x1, x2;     /* Input history */
    float y1, y2;     /* Output history */
    float b0, b1, b2;  /* Numerator coefficients */
    float a1, a2;      /* Denominator coefficients */
} biquad_t;

/* 0.2-2.0 Hz bandpass at 200 Hz sample rate */
/* Designed with scipy.signal.butter(2, [0.2, 2.0], btype='band', fs=200) */
static biquad_t bp_filter = {
    .b0 = 0.000390,
    .b1 = 0.0,
    .b2 = -0.000390,
    .a1 = -1.9786,
    .a2 = 0.9992,
    .x1 = 0, .x2 = 0, .y1 = 0, .y2 = 0
};

static inline float biquad_process(biquad_t *f, float x) {
    float y = f->b0 * x + f->b1 * f->x1 + f->b2 * f->x2
            - f->a1 * f->y1 - f->a2 * f->y2;
    f->x2 = f->x1; f->x1 = x;
    f->y2 = f->y1; f->y1 = y;
    return y;
}

/* ── System State ─────────────────────────────────────────────────────── */
typedef struct {
    /* Breathing */
    uint8_t  breath_rate;         /* Current breaths per minute */
    uint8_t  breath_regularity;   /* 0-100 regularity index */
    uint16_t last_breath_time_ms; /* Time of last detected breath peak */
    uint16_t apnea_count;         /* Apnea events in current window */
    float    fsr_sum_buffer[1000];/* Last 5 seconds of filtered FSR sum */
    uint16_t fsr_buffer_idx;
    
    /* Movement */
    uint8_t  movement_score;      /* Current movement intensity 0-255 */
    uint16_t movement_epochs;    /* Movement count in current 2s window */
    
    /* Position */
    uint8_t  position;            /* POS_* enum */
    
    /* Temperature */
    float    mattress_temp_c;
    
    /* Wetness */
    uint8_t  wetness_flag;
    uint8_t  wetness_level;
    
    /* FSR raw values */
    uint16_t fsr_raw[4];
    
    /* Timing */
    uint32_t last_tx_time_ms;
    uint32_t last_sample_time_ms;
    uint8_t  alert_level;
    uint8_t  battery_pct;
    
    /* ADC DMA buffer */
    uint16_t adc_buffer[6];  /* FSR1-4, Wet1-2 */
} crib_state_t;

crib_state_t state;
ADC_HandleTypeDef hadc1;
SPI_HandleTypeDef hspi1;
I2C_HandleTypeDef hi2c1;

/* ── FSR and ADC Initialization ────────────────────────────────────────── */
void adc_init(void) {
    /* Configure ADC for continuous scan of 6 channels */
    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV4;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = ENABLE;
    hadc1.Init.ContinuousConvMode = ENABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 6;
    hadc1.Init.DMAContinuousRequests = ENABLE;
    HAL_ADC_Init(&hadc1);
    
    /* Configure channels */
    ADC_ChannelConfTypeDef sConfig = {0};
    
    /* FSR1 - Channel 5 (PA0) */
    sConfig.Channel = ADC_CHANNEL_5;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    
    /* FSR2 - Channel 6 (PA1) */
    sConfig.Channel = ADC_CHANNEL_6;
    sConfig.Rank = 2;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    
    /* FSR3 - Channel 7 (PA2) */
    sConfig.Channel = ADC_CHANNEL_7;
    sConfig.Rank = 3;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    
    /* FSR4 - Channel 8 (PA3) */
    sConfig.Channel = ADC_CHANNEL_8;
    sConfig.Rank = 4;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    
    /* Wet1 - Channel 14 (PA4) */
    sConfig.Channel = ADC_CHANNEL_14;
    sConfig.Rank = 5;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    
    /* Wet2 - Channel 15 (PA5) */
    sConfig.Channel = ADC_CHANNEL_15;
    sConfig.Rank = 6;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
}

/* ── LIS3DH Accelerometer ──────────────────────────────────────────────── */
#define LIS3DH_ADDR 0x18

typedef struct {
    float x, y, z;
} accel_data_t;

int lis3dh_read(accel_data_t *data) {
    uint8_t buf[6];
    uint8_t reg = 0x28 | 0x80;  /* OUT_X_L with auto-increment */
    
    if (HAL_I2C_Master_Transmit(&hi2c1, LIS3DH_ADDR << 1, &reg, 1, 10) != HAL_OK)
        return -1;
    if (HAL_I2C_Master_Receive(&hi2c1, LIS3DH_ADDR << 1, buf, 6, 10) != HAL_OK)
        return -1;
    
    int16_t x = (int16_t)(buf[0] | (buf[1] << 8));
    int16_t y = (int16_t)(buf[2] | (buf[3] << 8));
    int16_t z = (int16_t)(buf[4] | (buf[5] << 8));
    
    /* Convert to g (±4g range, 1mg/LSB at 4g full-scale) */
    data->x = x * 0.001f;
    data->y = y * 0.001f;
    data->z = z * 0.001f;
    
    return 0;
}

uint8_t lis3dh_detect_position(const accel_data_t *accel) {
    /* Detect baby position from gravity vector */
    float abs_x = accel->x < 0 ? -accel->x : accel->x;
    float abs_y = accel->y < 0 ? -accel->y : accel->y;
    float abs_z = accel->z < 0 ? -accel->z : accel->z;
    
    float max_val = abs_x;
    char max_axis = 'x';
    if (abs_y > max_val) { max_val = abs_y; max_axis = 'y'; }
    if (abs_z > max_val) { max_val = abs_z; max_axis = 'z'; }
    
    /* Gravity along Z = supine (on back) */
    if (max_axis == 'z') {
        return (accel->z > 0) ? POS_SUPINE : POS_PRONE;
    }
    /* Gravity along X or Y = side sleeping */
    if (max_axis == 'x') {
        return (accel->x > 0) ? POS_LEFT_SIDE : POS_RIGHT_SIDE;
    }
    /* Gravity along Y = semi-upright */
    return POS_SEMI_UPRIGHT;
}

/* ── SHT40 Temperature Sensor ──────────────────────────────────────────── */
#define SHT40_ADDR 0x44

int sht40_read(float *temp_c, float *humidity_pct) {
    uint8_t cmd = 0xFD;  /* High-precision measurement */
    if (HAL_I2C_Master_Transmit(&hi2c1, SHT40_ADDR << 1, &cmd, 1, 10) != HAL_OK)
        return -1;
    HAL_Delay(10);  /* 10ms measurement time */
    
    uint8_t buf[6];
    if (HAL_I2C_Master_Receive(&hi2c1, SHT40_ADDR << 1, buf, 6, 10) != HAL_OK)
        return -1;
    
    uint16_t temp_raw = (buf[0] << 8) | buf[1];
    uint16_t hum_raw = (buf[3] << 8) | buf[4];
    
    *temp_c = -45.0f + 175.0f * (float)temp_raw / 65535.0f;
    *humidity_pct = 0.0f + 100.0f * (float)hum_raw / 65535.0f;
    
    return 0;
}

/* ── BCG Breathing Extraction ───────────────────────────────────────────── */
/**
 * Extract breathing rate from force sensor data using BCG.
 * 
 * Algorithm:
 * 1. Sum all 4 FSR channels to get total force signal
 * 2. Bandpass filter (0.2-2.0 Hz) to isolate breathing component
 * 3. Detect peaks using threshold crossing
 * 4. Count peaks per minute = breath rate
 * 5. Measure peak-to-peak regularity
 */
void process_bcg_samples(void) {
    /* Read all 4 FSR channels */
    uint16_t fsr_sum = state.adc_buffer[0] + state.adc_buffer[1] 
                     + state.adc_buffer[2] + state.adc_buffer[3];
    
    /* Store raw values */
    state.fsr_raw[0] = state.adc_buffer[0];
    state.fsr_raw[1] = state.adc_buffer[1];
    state.fsr_raw[2] = state.adc_buffer[2];
    state.fsr_raw[3] = state.adc_buffer[3];
    
    /* Bandpass filter the signal */
    float filtered = biquad_process(&bp_filter, (float)fsr_sum);
    
    /* Store in circular buffer */
    state.fsr_sum_buffer[state.fsr_buffer_idx] = filtered;
    state.fsr_buffer_idx = (state.fsr_buffer_idx + 1) % 1000;
    
    /* Peak detection */
    static float prev_sample = 0, prev_prev_sample = 0;
    static uint32_t last_peak_time_ms = 0;
    static float peak_threshold = 50.0f;
    
    uint32_t now = HAL_GetTick();
    
    /* Detect peak: sample is higher than neighbors and above threshold */
    if (prev_prev_sample < prev_sample && prev_sample > filtered && prev_sample > peak_threshold) {
        /* Peak detected at prev_sample */
        uint32_t peak_interval_ms = now - last_peak_time_ms;
        
        /* Validate: breathing interval should be 0.5-5 seconds */
        if (peak_interval_ms > 500 && peak_interval_ms < 5000 && last_peak_time_ms > 0) {
            /* Calculate instantaneous breath rate */
            float instant_rate = 60000.0f / (float)peak_interval_ms;
            
            /* Smooth with exponential moving average */
            if (state.breath_rate == 0) {
                state.breath_rate = (uint8_t)(instant_rate + 0.5f);
            } else {
                float smoothed = 0.7f * state.breath_rate + 0.3f * instant_rate;
                state.breath_rate = (uint8_t)(smoothed + 0.5f);
            }
            
            /* Calculate regularity (coefficient of variation) */
            static float intervals[10];
            static uint8_t interval_idx = 0;
            intervals[interval_idx] = (float)peak_interval_ms;
            interval_idx = (interval_idx + 1) % 10;
            
            float mean = 0;
            for (int i = 0; i < 10; i++) mean += intervals[i];
            mean /= 10.0f;
            
            float variance = 0;
            for (int i = 0; i < 10; i++) {
                float diff = intervals[i] - mean;
                variance += diff * diff;
            }
            variance /= 10.0f;
            
            float std_dev = variance > 0 ? sqrtf(variance) : 0;
            float cv = (mean > 0) ? (std_dev / mean) : 1.0f;
            
            /* Regularity: 100 = perfectly regular, 0 = highly irregular */
            state.breath_regularity = (uint8_t)(100.0f * (1.0f - (cv > 1.0f ? 1.0f : cv)));
            
            /* Track apnea */
            if (peak_interval_ms > APNEA_THRESHOLD_MS) {
                state.apnea_count++;
            }
        }
        
        last_peak_time_ms = now;
    }
    
    prev_prev_sample = prev_sample;
    prev_sample = filtered;
    
    /* Movement detection */
    static float prev_fsr_sum = 0;
    float delta = (float)fsr_sum - prev_fsr_sum;
    if (delta < 0) delta = -delta;
    
    if (delta > MOVEMENT_THRESHOLD) {
        state.movement_epochs++;
        state.movement_score = (uint8_t)(delta > 255 ? 255 : delta);
    }
    prev_fsr_sum = (float)fsr_sum;
    
    /* Wetness detection */
    state.wetness_level = (uint8_t)((state.adc_buffer[4] + state.adc_buffer[5]) / 2 / 16);
    state.wetness_flag = (state.wetness_level > WETNESS_THRESHOLD) ? 1 : 0;
}

/* ── Battery Monitoring ────────────────────────────────────────────────── */
uint8_t read_battery_pct(void) {
    /* Read CR2450 battery voltage via ADC */
    /* VBAT is measured through a voltage divider (2:1) on PA6 */
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    uint32_t vbat_raw = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    
    float vbat_mv = (float)vbat_raw * 3300.0f / 4096.0f * 2.0f;  /* Divider ratio */
    
    /* CR2450: 3.0V nominal, 2.0V dead */
    if (vbat_mv > 3000) return 100;
    if (vbat_mv < 2000) return 0;
    return (uint8_t)((vbat_mv - 2000) / 10);  /* Linear: 2000mV=0%, 3000mV=100% */
}

/* ── Transmit Data to Hub ──────────────────────────────────────────────── */
void transmit_to_hub(void) {
    crib_data_t data = {
        .breath_rate = state.breath_rate,
        .breath_regularity = state.breath_regularity,
        .movement_score = state.movement_score,
        .position = state.position,
        .temp_c_x10 = (int16_t)(state.mattress_temp_c * 10),
        .wetness_flag = state.wetness_flag,
        .wetness_level = state.wetness_level,
        .breath_apnea_count = state.apnea_count,
        .movement_epochs = state.movement_epochs,
        .alert_level = state.alert_level,
        .battery_pct = state.battery_pct,
        .signal_strength = 0,  /* Filled by radio driver */
        .fsr1_raw_h = (state.fsr_raw[0] >> 8) & 0xFF,
        .fsr1_raw_l = state.fsr_raw[0] & 0xFF,
        .fsr2_raw_h = (state.fsr_raw[1] >> 8) & 0xFF,
        .fsr2_raw_l = state.fsr_raw[1] & 0xFF,
        .fsr3_raw_h = (state.fsr_raw[2] >> 8) & 0xFF,
        .fsr3_raw_l = state.fsr_raw[2] & 0xFF,
        .fsr4_raw_h = (state.fsr_raw[3] >> 8) & 0xFF,
        .fsr4_raw_l = state.fsr_raw[3] & 0xFF,
    };
    
    packet_t pkt = {
        .src = ADDR_CRIB_PAD,
        .dst = ADDR_HUB,
        .type = PKT_CRIB_DATA,
        .payload_len = sizeof(data),
    };
    memcpy(pkt.payload, &data, sizeof(data));
    
    /* Use faster spreading factor if in alert */
    if (state.alert_level >= ALERT_URGENT) {
        radio_set_spreading_factor(9);  /* Longer range, more reliable */
    }
    
    radio_send(&pkt);
    
    radio_set_spreading_factor(7);  /* Reset to normal */
}

/* ── Determine Alert Level ─────────────────────────────────────────────── */
void update_alert_level(void) {
    /* Check for apnea (no breathing) */
    uint32_t now = HAL_GetTick();
    uint32_t time_since_breath = now - state.last_breath_time_ms;
    
    if (time_since_breath > 15000) {
        state.alert_level = ALERT_EMERGENCY;
    } else if (time_since_breath > 10000) {
        state.alert_level = ALERT_URGENT;
    } else if (time_since_breath > 5000) {
        state.alert_level = ALERT_WARNING;
    } else if (state.breath_rate > 0 && 
              (state.breath_rate < BREATH_RATE_MIN || state.breath_rate > BREATH_RATE_MAX)) {
        state.alert_level = ALERT_WARNING;
    } else {
        state.alert_level = ALERT_INFO;
    }
}

/* ── Main Loop ─────────────────────────────────────────────────────────── */
int main(void) {
    HAL_Init();
    
    /* Initialize peripherals */
    adc_init();
    
    /* Initialize I2C for SHT40 + LIS3DH */
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 100000;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    HAL_I2C_Init(&hi2c1);
    
    /* Initialize SPI for SX1261 */
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
    HAL_SPI_Init(&hspi1);
    
    /* Initialize radio */
    radio_config_t radio_cfg = {
        .address = ADDR_CRIB_PAD,
        .frequency = 868000000,
        .spreading_factor = 7,
        .bandwidth = 4,
        .coding_rate = 1,
        .tx_power = 14,
        .preamble_len = 8,
        .sync_word = 0x0C4B,
    };
    radio_init(&radio_cfg);
    
    /* Initialize state */
    memset(&state, 0, sizeof(state));
    state.last_sample_time_ms = HAL_GetTick();
    
    /* Initialize LIS3DH */
    uint8_t lis3dh_init_cmd[] = {0x20, 0x57};  /* CTRL1: 50Hz, normal mode, all axes */
    HAL_I2C_Master_Transmit(&hi2c1, LIS3DH_ADDR << 1, lis3dh_init_cmd, 2, 10);
    
    /* Main loop */
    while (1) {
        uint32_t now = HAL_GetTick();
        
        /* Sample FSR sensors at 200Hz (every 5ms) */
        if (now - state.last_sample_time_ms >= 5) {
            /* Start ADC conversion */
            HAL_ADC_Start(&hadc1);
            for (int i = 0; i < 6; i++) {
                HAL_ADC_PollForConversion(&hadc1, 1);
                state.adc_buffer[i] = HAL_ADC_GetValue(&hadc1);
            }
            HAL_ADC_Stop(&hadc1);
            
            /* Process BCG samples */
            process_bcg_samples();
            
            state.last_sample_time_ms = now;
        }
        
        /* Read position from accelerometer (every 500ms) */
        static uint32_t last_accel_time = 0;
        if (now - last_accel_time >= 500) {
            accel_data_t accel;
            if (lis3dh_read(&accel) == 0) {
                state.position = lis3dh_detect_position(&accel);
            }
            last_accel_time = now;
        }
        
        /* Read temperature (every 5 seconds) */
        static uint32_t last_temp_time = 0;
        if (now - last_temp_time >= 5000) {
            float temp, hum;
            if (sht40_read(&temp, &hum) == 0) {
                state.mattress_temp_c = temp;
            }
            last_temp_time = now;
        }
        
        /* Update alert level */
        update_alert_level();
        
        /* Determine transmit interval based on alert level */
        uint32_t tx_interval = (state.alert_level >= ALERT_WARNING) 
                               ? TX_INTERVAL_ALERT_MS 
                               : TX_INTERVAL_NORMAL_MS;
        
        /* Transmit data to hub */
        if (now - state.last_tx_time_ms >= tx_interval) {
            state.battery_pct = read_battery_pct();
            transmit_to_hub();
            state.last_tx_time_ms = now;
            
            /* Reset movement epochs counter after transmission */
            state.movement_epochs = 0;
        }
        
        /* Ultra-low-power sleep between samples */
        /* STM32L476: ~10µA in Stop mode with RTC */
        if (state.alert_level == ALERT_INFO) {
            HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
        }
    }
}