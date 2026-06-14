/**
 * UrbanHarvest - Weather Station Node Firmware
 * RP2040 + SX1262 + anemometer + rain gauge + environmental sensors
 *
 * Monitors outdoor conditions for irrigation optimization:
 * - Wind speed and direction
 * - Rainfall (tipping bucket)
 * - Temperature, humidity, pressure
 * - UV index
 * - Ambient light
 * - Solar power management
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/rtc.h"

/* ========== CONSTANTS ========== */
#define I2C0_SDA_PIN            0
#define I2C0_SCL_PIN            1
#define I2C1_SDA_PIN            2
#define I2C1_SCL_PIN            3
#define SPI_SCK_PIN             4
#define SPI_MISO_PIN            5
#define SPI_MOSI_PIN            6
#define SX1262_NSS_PIN          7
#define SX1262_BUSY_PIN         8
#define SX1262_IRQ_PIN          9
#define SX1262_NRST_PIN         10
#define ANEMOMETER_PIN          11
#define WIND_VANE_PIN           26  /* ADC0 */
#define RAIN_GAUGE_PIN          13
#define SOLAR_VOLT_PIN          27  /* ADC1 */
#define BAT_VOLT_PIN            28  /* ADC2 */
#define CHG_STATUS_PIN          16
#define BQ25570_EN_PIN          17
#define LED_R_PIN               18
#define LED_G_PIN               19
#define LED_B_PIN               20
#define BTN_PIN                 21

/* Anemometer calibration: rotations/second × 2.4 = km/h (Davis formula) */
#define ANEMOMETER_FACTOR       2.4f
#define ANEMOMETER_DEBOUNCE_US  15000  /* 15ms debounce */

/* Rain gauge: each tip = 0.2794mm (0.01 inches) */
#define RAIN_MM_PER_TIP         0.2794f
#define RAIN_DEBOUNCE_US        30000  /* 30ms debounce */

/* Wind vane ADC lookup table (voltage → direction) */
/* 8-direction potentiometer with 10kΩ reference divider */
/* ADC values for N, NE, E, SE, S, SW, W, NW */
static const uint16_t wind_vane_table[8] = {
    3200, 2400, 1600, 800, 500, 1100, 2000, 2800
};
static const char *wind_dir_names[8] = {
    "N", "NE", "E", "SE", "S", "SW", "W", "NW"
};

/* Sampling intervals */
#define SENSOR_INTERVAL_MS      60000   /* 60 seconds */
#define MESH_TX_INTERVAL_MS     60000   /* 60 seconds */
#define BAROMETRIC_TREND_MS     10800000 /* 3 hours for pressure trend */

/* ========== DATA STRUCTURES ========== */

typedef struct __attribute__((packed)) {
    uint8_t  preamble[2];
    uint8_t  len;
    uint8_t  src_id;
    uint8_t  dst_id;
    uint8_t  msg_type;
    uint16_t seq_num;
    uint8_t  payload[48];
    uint16_t crc16;
} urbanharvest_packet_t;

typedef struct {
    float temperature_c;
    float humidity_pct;
    float pressure_hpa;
    float wind_speed_kmh;
    uint8_t wind_direction;    /* 0-7: N,NE,E,SE,S,SW,W,NW */
    float rain_mm;
    float rain_rate_mm_h;
    float uv_index;
    uint16_t light_lux;
    float solar_voltage;
    float battery_voltage;
    uint8_t battery_soc_pct;
    int8_t pressure_trend;     /* -1=falling, 0=steady, 1=rising */
    uint32_t timestamp_ms;
} weather_readings_t;

/* ========== GLOBALS ========== */

static weather_readings_t weather;
static uint16_t mesh_seq = 0;

/* Anemometer counting */
static volatile uint32_t anemometer_count = 0;
static uint32_t anemometer_last_us = 0;
static uint32_t anemometer_interval_start_us = 0;
static uint32_t anemometer_count_at_interval = 0;

/* Rain gauge counting */
static volatile uint32_t rain_tip_count = 0;
static uint32_t rain_last_us = 0;
static uint32_t rain_count_at_interval = 0;

/* Barometric trend */
static float pressure_3h_ago_hpa = 0;
static absolute_time_t pressure_3h_time;

/* ========== INTERRUPT HANDLERS ========== */

/**
 * anemometer_isr - Called on each anemometer reed switch pulse
 * Counts rotations for wind speed calculation
 */
void anemometer_isr(void)
{
    uint32_t now = time_us_32();

    /* Debounce: ignore pulses within 15ms */
    if ((now - anemometer_last_us) < ANEMOMETER_DEBOUNCE_US) {
        return;
    }
    anemometer_last_us = now;
    anemometer_count++;
}

/**
 * rain_gauge_isr - Called on each tipping bucket pulse
 * Each tip = 0.2794mm rainfall
 */
void rain_gauge_isr(void)
{
    uint32_t now = time_us_32();

    /* Debounce: ignore pulses within 30ms */
    if ((now - rain_last_us) < RAIN_DEBOUNCE_US) {
        return;
    }
    rain_last_us = now;
    rain_tip_count++;
}

/* ========== SENSOR READERS ========== */

/**
 * read_anemometer - Calculate wind speed from rotation count
 * Wind speed = rotations/second × calibration_factor
 */
static float read_anemometer(void)
{
    uint32_t now = time_us_32();
    uint32_t interval_us = now - anemometer_interval_start_us;
    uint32_t rotations = anemometer_count - anemometer_count_at_interval;

    /* Save interval start for next calculation */
    anemometer_count_at_interval = anemometer_count;
    anemometer_interval_start_us = now;

    if (interval_us == 0) return 0.0f;

    float rotations_per_sec = (float)rotations / (float)(interval_us / 1000000);
    float wind_speed_kmh = rotations_per_sec * ANEMOMETER_FACTOR;

    return wind_speed_kmh;
}

/**
 * read_wind_vane - Read wind direction from potentiometer
 * ADC → voltage → closest direction in lookup table
 */
static uint8_t read_wind_vane(void)
{
    uint16_t adc = adc_read();  /* ADC0 = GPIO26 */

    /* Find closest match in lookup table */
    uint8_t best_dir = 0;
    uint16_t best_diff = 65535;
    for (int i = 0; i < 8; i++) {
        uint16_t diff = abs((int)adc - (int)wind_vane_table[i]);
        if (diff < best_diff) {
            best_diff = diff;
            best_dir = i;
        }
    }

    return best_dir;
}

/**
 * read_rain_gauge - Calculate rainfall from tipping bucket count
 * Returns total rainfall since last call, in mm
 */
static float read_rain_gauge(void)
{
    uint32_t tips = rain_tip_count - rain_count_at_interval;
    rain_count_at_interval = rain_tip_count;
    return (float)tips * RAIN_MM_PER_TIP;
}

/**
 * read_sht45 - Read temperature and humidity from SHT45
 * High accuracy: ±0.2°C, ±1.8% RH
 */
static void read_sht45(float *temp_c, float *rh_pct)
{
    /* TODO: I2C0 communication with SHT45
     * 1. Send measurement command (0xFD = high precision)
     * 2. Wait ~10ms for measurement
     * 3. Read 6 bytes: temp_msb, temp_lsb, crc, rh_msb, rh_lsb, crc
     * 4. Convert: temp = -45 + 175 * (raw / 65535)
     *             rh   =    0 + 100 * (raw / 65535)
     */

    /* Placeholder values */
    *temp_c = 22.0f;
    *rh_pct = 55.0f;
}

/**
 * read_bmp390 - Read barometric pressure from BMP390
 * Accuracy: ±0.5 hPa
 */
static float read_bmp390(void)
{
    /* TODO: I2C0 communication with BMP390
     * 1. Read calibration coefficients from NVM
     * 2. Trigger forced measurement
     * 3. Read raw pressure and temperature
     * 4. Apply BMP390 compensation formula
     */

    /* Placeholder */
    return 1013.25f;
}

/**
 * read_veml6075 - Read UV index from VEML6075
 * UVA + UVB → UV index calculation
 */
static float read_veml6075(void)
{
    /* TODO: I2C1 communication with VEML6075
     * 1. Read UVA raw (0x07)
     * 2. Read UVB raw (0x09)
     * 3. Read UV_COMP1 (0x0A) and UV_COMP2 (0x0B)
     * 4. Apply compensation: UVA_comp = UVA - a*UV_COMP1 - b*UV_COMP2
     * 5. UV index = (UVA_comp × a_resp + UVB_comp × b_resp) / (2 × sensitivity)
     */

    /* Placeholder */
    return 3.5f;
}

/**
 * read_battery_soc - Estimate battery state of charge
 * LiPo discharge curve: 4.2V=100%, 3.7V=50%, 3.0V=0%
 */
static uint8_t read_battery_soc(float voltage)
{
    if (voltage >= 4.2f) return 100;
    if (voltage >= 4.0f) return (uint8_t)(50.0f + 50.0f * (voltage - 4.0f) / 0.2f);
    if (voltage >= 3.7f) return (uint8_t)(20.0f + 30.0f * (voltage - 3.7f) / 0.3f);
    if (voltage >= 3.3f) return (uint8_t)(5.0f + 15.0f * (voltage - 3.3f) / 0.4f);
    if (voltage >= 3.0f) return (uint8_t)(5.0f * (voltage - 3.0f) / 0.3f);
    return 0;
}

/**
 * calculate_pressure_trend - Determine if pressure is rising/falling/steady
 * Compares current pressure to 3-hour-ago reading
 * Falling pressure + high humidity = rain likely
 */
static int8_t calculate_pressure_trend(float current_hpa)
{
    if (pressure_3h_ago_hpa == 0) {
        /* No 3-hour reference yet */
        return 0;
    }

    float delta = current_hpa - pressure_3h_ago_hpa;

    if (delta > 1.0f) return 1;    /* Rising */
    if (delta < -1.0f) return -1;  /* Falling */
    return 0;                       /* Steady */
}

/**
 * predict_rain - Simple rain prediction from barometric trend + humidity
 * Not as accurate as professional forecasting, but useful for irrigation skip
 * Returns 1 if rain is likely within 2-6 hours
 */
static int predict_rain(void)
{
    /* Falling barometric pressure is a strong rain indicator */
    if (weather.pressure_trend < 0 && weather.pressure_hpa < 1010.0f) {
        if (weather.humidity_pct > 75.0f) {
            return 1;  /* Rain likely */
        }
    }

    /* Rapid pressure drop (> 2 hPa in 3 hours) */
    float delta = weather.pressure_hpa - pressure_3h_ago_hpa;
    if (delta < -2.0f) {
        return 1;
    }

    return 0;
}

/* ========== MESH COMMUNICATION ========== */

/**
 * crc16_ccitt - Calculate CRC16-CCITT
 */
static uint16_t crc16_ccitt(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else crc <<= 1;
        }
    }
    return crc;
}

/**
 * mesh_send_weather - Send weather data to hub
 */
static void mesh_send_weather(void)
{
    urbanharvest_packet_t pkt;
    pkt.preamble[0] = 0xAA;
    pkt.preamble[1] = 0x55;
    pkt.src_id = 0x80;  /* Weather station ID range: 0x80 */
    pkt.dst_id = 0;      /* Hub */
    pkt.msg_type = 0x05;  /* WEATHER_DATA */
    pkt.seq_num = mesh_seq++;

    /* Pack weather data */
    int idx = 0;
    /* Temperature ×10 (int16): -40.0 to +85.0°C */
    int16_t temp_x10 = (int16_t)(weather.temperature_c * 10.0f);
    pkt.payload[idx++] = (temp_x10 >> 8) & 0xFF;
    pkt.payload[idx++] = temp_x10 & 0xFF;
    /* Humidity ×10 */
    uint16_t rh_x10 = (uint16_t)(weather.humidity_pct * 10.0f);
    pkt.payload[idx++] = (rh_x10 >> 8) & 0xFF;
    pkt.payload[idx++] = rh_x10 & 0xFF;
    /* Pressure ×10 (hPa) */
    uint16_t pres_x10 = (uint16_t)(weather.pressure_hpa * 10.0f);
    pkt.payload[idx++] = (pres_x10 >> 8) & 0xFF;
    pkt.payload[idx++] = pres_x10 & 0xFF;
    /* Wind speed ×10 (km/h) */
    uint16_t wind_x10 = (uint16_t)(weather.wind_speed_kmh * 10.0f);
    pkt.payload[idx++] = (wind_x10 >> 8) & 0xFF;
    pkt.payload[idx++] = wind_x10 & 0xFF;
    /* Wind direction */
    pkt.payload[idx++] = weather.wind_direction;
    /* Rain ×100 (mm) */
    uint16_t rain_x100 = (uint16_t)(weather.rain_mm * 100.0f);
    pkt.payload[idx++] = (rain_x100 >> 8) & 0xFF;
    pkt.payload[idx++] = rain_x100 & 0xFF;
    /* UV ×10 */
    uint16_t uv_x10 = (uint16_t)(weather.uv_index * 10.0f);
    pkt.payload[idx++] = (uv_x10 >> 8) & 0xFF;
    pkt.payload[idx++] = uv_x10 & 0xFF;
    /* Light (lux, direct) */
    pkt.payload[idx++] = (weather.light_lux >> 8) & 0xFF;
    pkt.payload[idx++] = weather.light_lux & 0xFF;
    /* Solar voltage ×20 */
    pkt.payload[idx++] = (uint8_t)(weather.solar_voltage * 20.0f);
    /* Battery voltage ×20 */
    pkt.payload[idx++] = (uint8_t)(weather.battery_voltage * 20.0f);
    /* Battery SOC */
    pkt.payload[idx++] = weather.battery_soc_pct;
    /* Pressure trend: -1, 0, 1 → encoded as 1, 2, 3 */
    pkt.payload[idx++] = (uint8_t)(weather.pressure_trend + 2);
    /* Rain prediction flag */
    pkt.payload[idx++] = predict_rain();

    pkt.len = sizeof(urbanharvest_packet_t);
    pkt.crc16 = crc16_ccitt((const uint8_t *)&pkt.src_id,
                             sizeof(urbanharvest_packet_t) - 4);

    /* TODO: SX1262 transmit via SPI0 */

    printf("TX: temp=%.1f rh=%.0f pres=%.1f wind=%.1f%s rain=%.1f uv=%.1f rain_pred=%d\n",
           weather.temperature_c, weather.humidity_pct, weather.pressure_hpa,
           weather.wind_speed_kmh, wind_dir_names[weather.wind_direction],
           weather.rain_mm, weather.uv_index, predict_rain());
}

/* ========== MAIN ========== */

int main(void)
{
    stdio_init_all();
    printf("UrbanHarvest Weather Station starting...\n");

    /* Initialize I2C0: SHT45 + BMP390 */
    i2c_init(i2c0, 100000);
    gpio_set_function(I2C0_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C0_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C0_SDA_PIN);
    gpio_pull_up(I2C0_SCL_PIN);

    /* Initialize I2C1: VEML6075 + TSL25911 */
    i2c_init(i2c1, 100000);
    gpio_set_function(I2C1_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C1_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C1_SDA_PIN);
    gpio_pull_up(I2C1_SCL_PIN);

    /* Initialize ADC for wind vane, solar, battery */
    adc_init();
    adc_gpio_init(WIND_VANE_PIN);    /* ADC0 */
    adc_gpio_init(SOLAR_VOLT_PIN);   /* ADC1 */
    adc_gpio_init(BAT_VOLT_PIN);     /* ADC2 */

    /* Initialize anemometer GPIO interrupt */
    gpio_init(ANEMOMETER_PIN);
    gpio_set_dir(ANEMOMETER_PIN, GPIO_IN);
    gpio_pull_up(ANEMOMETER_PIN);
    gpio_set_irq_enabled_with_callback(ANEMOMETER_PIN,
        GPIO_IRQ_EDGE_FALL, true, &anemometer_isr);

    /* Initialize rain gauge GPIO interrupt */
    gpio_init(RAIN_GAUGE_PIN);
    gpio_set_dir(RAIN_GAUGE_PIN, GPIO_IN);
    gpio_pull_up(RAIN_GAUGE_PIN);
    gpio_set_irq_enabled_with_callback(RAIN_GAUGE_PIN,
        GPIO_IRQ_EDGE_FALL, true, &rain_gauge_isr);

    /* Initialize SX1262 SPI + control pins */
    spi_init(spi0, 4000000);  /* 4MHz SPI */
    gpio_set_function(SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_MISO_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_init(SX1262_NSS_PIN);
    gpio_set_dir(SX1262_NSS_PIN, GPIO_OUT);
    gpio_put(SX1262_NSS_PIN, 1);
    gpio_init(SX1262_BUSY_PIN);
    gpio_set_dir(SX1262_BUSY_PIN, GPIO_IN);
    gpio_init(SX1262_IRQ_PIN);
    gpio_set_dir(SX1262_IRQ_PIN, GPIO_IN);
    gpio_init(SX1262_NRST_PIN);
    gpio_set_dir(SX1262_NRST_PIN, GPIO_OUT);
    gpio_put(SX1262_NRST_PIN, 1);

    /* Initialize status LEDs */
    gpio_init(LED_R_PIN);
    gpio_set_dir(LED_R_PIN, GPIO_OUT);
    gpio_init(LED_G_PIN);
    gpio_set_dir(LED_G_PIN, GPIO_OUT);
    gpio_init(LED_B_PIN);
    gpio_set_dir(LED_B_PIN, GPIO_OUT);

    /* Initialize energy harvester enable */
    gpio_init(BQ25570_EN_PIN);
    gpio_set_dir(BQ25570_EN_PIN, GPIO_OUT);
    gpio_put(BQ25570_EN_PIN, 1);  /* Enable BQ25570 */

    /* Initialize timing */
    anemometer_interval_start_us = time_us_32();
    pressure_3h_time = get_absolute_time();

    printf("Weather station ready — sensors initialized\n");

    /* Green LED blink: system OK */
    gpio_put(LED_G_PIN, 1);

    uint32_t last_sensor_ms = 0;
    uint32_t last_tx_ms = 0;

    while (1) {
        uint32_t now = to_ms_since_boot(get_absolute_time());

        /* Sensor reading cycle (every 60 seconds) */
        if ((now - last_sensor_ms) >= SENSOR_INTERVAL_MS) {
            /* Read all sensors */
            read_sht45(&weather.temperature_c, &weather.humidity_pct);
            weather.pressure_hpa = read_bmp390();
            weather.wind_speed_kmh = read_anemometer();
            weather.wind_direction = read_wind_vane();
            weather.rain_mm = read_rain_gauge();
            weather.rain_rate_mm_h = weather.rain_mm * 60.0f;  /* Scale to mm/hr */
            weather.uv_index = read_veml6075();

            /* Read power system */
            adc_select_input(1);  /* Solar voltage ADC */
            uint16_t solar_adc = adc_read();
            weather.solar_voltage = (float)solar_adc * 2.0f * 3.3f / 4095.0f;

            adc_select_input(2);  /* Battery voltage ADC */
            uint16_t bat_adc = adc_read();
            weather.battery_voltage = (float)bat_adc * 2.0f * 3.3f / 4095.0f;
            weather.battery_soc_pct = read_battery_soc(weather.battery_voltage);

            /* Pressure trend (3-hour comparison) */
            if ((now - to_ms_since_boot(pressure_3h_time)) >= BAROMETRIC_TREND_MS) {
                weather.pressure_trend = calculate_pressure_trend(weather.pressure_hpa);
                pressure_3h_ago_hpa = weather.pressure_hpa;
                pressure_3h_time = get_absolute_time();
            }

            weather.timestamp_ms = now;
            last_sensor_ms = now;

            /* Update status LED based on battery */
            if (weather.battery_soc_pct < 20) {
                gpio_put(LED_R_PIN, 1);
                gpio_put(LED_G_PIN, 0);
            } else {
                gpio_put(LED_R_PIN, 0);
                gpio_put(LED_G_PIN, 1);
            }
        }

        /* Mesh TX cycle (every 60 seconds) */
        if ((now - last_tx_ms) >= MESH_TX_INTERVAL_MS) {
            mesh_send_weather();
            last_tx_ms = now;
        }

        /* Main loop: sleep 1 second */
        sleep_ms(1000);
    }

    return 0;
}