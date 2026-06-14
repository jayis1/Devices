/**
 * UrbanHarvest - Plant Sensor Node Firmware
 * STM32WL55CC (integrated Sub-GHz)
 *
 * Measures: soil moisture, soil EC, soil temperature, PAR light, leaf wetness
 * Sends readings to hub via Sub-GHz mesh TDMA
 * On-board TFLite Micro plant health classifier
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/uart.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ========== CONSTANTS ========== */
#define NODE_ID                 0x01    /* Assigned by hub during pairing, default 1 */
#define MESH_SLOT_DURATION_MS   100
#define MESH_FRAME_DURATION_MS  2600
#define MESH_NUM_SLOTS          26

/* Sensor sampling intervals */
#define MOISTURE_INTERVAL_MS    300000  /* 5 minutes */
#define EC_INTERVAL_MS          900000  /* 15 minutes */
#define TEMP_INTERVAL_MS        900000  /* 15 minutes */
#define LIGHT_INTERVAL_MS       600000  /* 10 minutes */
#define LEAF_WET_INTERVAL_MS    300000  /* 5 minutes */
#define MESH_TX_INTERVAL_MS     300000  /* 5 minutes (match fastest sensor) */

/* ADC channels */
#define ADC_MOISTURE_CHANNEL    5       /* PA0 / ADC_IN5 */
#define ADC_EC_MEAS_CHANNEL     7       /* PA2 / ADC_IN7 */
#define ADC_EC_REF_CHANNEL      8       /* PA3 / ADC_IN8 */
#define ADC_LEAF_WET_CHANNEL    11      /* PB11 */
#define ADC_VBAT_CHANNEL       0       /* PC0 */

/* Health index thresholds */
#define HEALTH_THRIVING         80
#define HEALTH_STRESSED         35
#define HEALTH_CRITICAL         10

/* Calibration constants (per-unit, set during setup) */
#define MOISTURE_DRY_ADC        3200    /* ADC reading in dry air (~0% moisture) */
#define MOISTURE_WET_ADC        1500   /* ADC reading in water (~100% moisture) */
#define EC_CALIBRATION_FACTOR   1.0f    /* Multiplier from KCl solution calibration */
#define LEAF_WET_DRY_ADC        3800   /* ADC reading when leaf surface is dry */
#define LEAF_WET_WET_ADC        2000   /* ADC reading when leaf surface is wet */

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
    float soil_moisture_pct;
    float soil_ec_ms_cm;
    float soil_temp_c;
    float par_umol_m2s;
    uint16_t light_lux;
    float leaf_wetness_pct;
    float leaf_wet_duration_h;
    uint8_t health_index;
    uint8_t health_category;
    float battery_voltage;
    uint32_t timestamp_ms;
} sensor_readings_t;

/* ========== GLOBALS ========== */

static sensor_readings_t readings;
static uint16_t mesh_seq = 0;
static uint8_t node_id = NODE_ID;
static uint8_t paired = 0;
static uint8_t slot_assigned = 0;

/* Leaf wetness tracking */
static uint32_t leaf_wet_start_ms = 0;
static float leaf_wet_accumulated_h = 0;
static uint8_t leaf_currently_wet = 0;

/* ========== ADC READING ========== */

/**
 * adc_read_channel_oversampled - Read ADC with 16x oversampling
 * Returns averaged ADC value (12-bit, 0-4095)
 */
static uint16_t adc_read_channel_oversampled(uint8_t channel)
{
    uint32_t sum = 0;
    uint16_t samples = 16;

    /* TODO: Configure ADC channel, trigger conversion 16 times */
    for (int i = 0; i < samples; i++) {
        /* adc_read(channel) — placeholder */
        sum += 2048;  /* Placeholder mid-range reading */
    }

    return (uint16_t)(sum / samples);
}

/* ========== SOIL MOISTURE ========== */

/**
 * read_soil_moisture - Read capacitive soil moisture sensor
 * Maps ADC value to 0-100% using calibration constants
 */
static float read_soil_moisture(void)
{
    uint16_t adc = adc_read_channel_oversampled(ADC_MOISTURE_CHANNEL);

    /* Map: dry (high ADC) = 0%, wet (low ADC) = 100% */
    float pct = 100.0f * (float)(MOISTURE_DRY_ADC - adc) /
                (float)(MOISTURE_DRY_ADC - MOISTURE_WET_ADC);

    /* Clamp to 0-100 */
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;

    return pct;
}

/* ========== SOIL EC ========== */

/**
 * read_soil_ec - Measure soil electrical conductivity
 * Uses 4-wire AC excitation method:
 * 1. Drive AC square wave on excitation electrodes (PA1)
 * 2. Measure differential voltage on sense electrodes (PA2/PA3)
 * 3. EC = measured_voltage / reference_voltage * calibration_factor
 */
static float read_soil_ec(void)
{
    /* Drive AC excitation: toggle PA1 at ~1kHz for 20 cycles */
    for (int i = 0; i < 20; i++) {
        /* TODO: gpio_pin_set(gpio_dev, PA1, 1); */
        k_busy_wait(500);  /* 500µs = 1kHz half-period */
        /* TODO: gpio_pin_set(gpio_dev, PA1, 0); */
        k_busy_wait(500);
    }

    /* Small delay for settling */
    k_busy_wait(100);

    /* Read measurement and reference ADCs */
    uint16_t meas_adc = adc_read_channel_oversampled(ADC_EC_MEAS_CHANNEL);
    uint16_t ref_adc = adc_read_channel_oversampled(ADC_EC_REF_CHANNEL);

    /* EC = (meas / ref) * calibration * scale */
    float ec = 0.0f;
    if (ref_adc > 0) {
        ec = ((float)meas_adc / (float)ref_adc) * EC_CALIBRATION_FACTOR * 2.0f;
    }

    /* Temperature compensation: EC increases ~2% per °C above 25°C */
    if (readings.soil_temp_c > 0) {
        float temp_factor = 1.0f + 0.02f * (readings.soil_temp_c - 25.0f);
        ec /= temp_factor;
    }

    if (ec < 0.0f) ec = 0.0f;
    if (ec > 5.0f) ec = 5.0f;  /* Safety cap */

    return ec;
}

/* ========== SOIL TEMPERATURE ========== */

/**
 * read_soil_temperature - Read DS18B20 OneWire temperature sensor
 * Waterproof probe, 30cm cable, pushed into root zone
 */
static float read_soil_temperature(void)
{
    /* TODO: OneWire protocol implementation for DS18B20
     * 1. Reset pulse (500µs low)
     * 2. Presence detect (wait for DS18B20 pull-low)
     * 3. Skip ROM command (0xCC)
     * 4. Convert T command (0x44)
     * 5. Wait 750ms for conversion
     * 6. Reset + Skip ROM + Read Scratchpad (0xBE)
     * 7. Read 9 bytes, extract temperature from bytes 0-1
     */

    /* Placeholder: return last known reading */
    return readings.soil_temp_c > 0 ? readings.soil_temp_c : 22.0f;
}

/* ========== PAR LIGHT SENSOR ========== */

/**
 * read_par_light - Read TSL25911 light sensor, convert to PAR
 * TSL25911 gives lux; approximate conversion to PAR:
 *   1 µmol/m²/s ≈ 54 lux (for full spectrum daylight)
 *   1 µmol/m²/s ≈ 35 lux (for cool white LED)
 */
static float read_par_light(void)
{
    /* TODO: I2C read from TSL25911 (CH0 + CH1 ADC values)
     * Lux = (CH0 - CH1) * integration_factor / (gain × integration_time)
     */

    /* Placeholder */
    uint16_t lux = 800;  /* Typical indoor grow light */
    float par = (float)lux / 35.0f;  /* LED conversion factor */

    readings.light_lux = lux;
    return par;
}

/* ========== LEAF WETNESS ========== */

/**
 * read_leaf_wetness - Read capacitive leaf wetness sensor
 * Also tracks cumulative wetness duration (fungal disease risk metric)
 */
static float read_leaf_wetness(void)
{
    uint16_t adc = adc_read_channel_oversampled(ADC_LEAF_WET_CHANNEL);

    /* Map: dry (high ADC) = 0%, wet (low ADC) = 100% */
    float pct = 100.0f * (float)(LEAF_WET_DRY_ADC - adc) /
                (float)(LEAF_WET_DRY_ADC - LEAF_WET_WET_ADC);

    if (pct < 0.0f) pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;

    /* Track wetness duration */
    uint8_t currently_wet = (pct > 50.0f) ? 1 : 0;
    uint32_t now = k_uptime_get_32();

    if (currently_wet && !leaf_currently_wet) {
        /* Just became wet */
        leaf_wet_start_ms = now;
        leaf_currently_wet = 1;
    } else if (currently_wet && leaf_currently_wet) {
        /* Still wet — accumulate */
        leaf_wet_accumulated_h = (float)(now - leaf_wet_start_ms) / 3600000.0f;
    } else if (!currently_wet && leaf_currently_wet) {
        /* Just dried */
        leaf_wet_accumulated_h = (float)(now - leaf_wet_start_ms) / 3600000.0f;
        leaf_currently_wet = 0;
    }

    readings.leaf_wetness_pct = pct;
    readings.leaf_wet_duration_h = leaf_wet_accumulated_h;

    return pct;
}

/* ========== BATTERY MONITORING ========== */

/**
 * read_battery_voltage - Check battery level
 * 3× AA alkaline: fresh = 4.5V, dead = 3.0V
 */
static float read_battery_voltage(void)
{
    /* Voltage divider: 100kΩ / 100kΩ → V_bat = ADC × 2 × 3.3 / 4095 */
    uint16_t adc = adc_read_channel_oversampled(ADC_VBAT_CHANNEL);
    float voltage = (float)adc * 2.0f * 3.3f / 4095.0f;
    return voltage;
}

/* ========== PLANT HEALTH INDEX ========== */

/**
 * calculate_health_index - Simple rule-based plant health from readings
 * Returns 0-100: 0=dead, 50=stressed, 100=thriving
 */
static uint8_t calculate_health_index(void)
{
    float health = 100.0f;

    /* Soil moisture: ideal 40-65% for most vegetables */
    if (readings.soil_moisture_pct < 20.0f) {
        health -= 40.0f;
    } else if (readings.soil_moisture_pct < 40.0f) {
        health -= (40.0f - readings.soil_moisture_pct) * 1.5f;
    } else if (readings.soil_moisture_pct > 80.0f) {
        health -= 25.0f;
    } else if (readings.soil_moisture_pct > 65.0f) {
        health -= (readings.soil_moisture_pct - 65.0f) * 0.8f;
    }

    /* EC: ideal 1.0-2.5 mS/cm */
    if (readings.soil_ec_ms_cm < 0.5f) {
        health -= 20.0f;
    } else if (readings.soil_ec_ms_cm > 3.0f) {
        health -= (readings.soil_ec_ms_cm - 3.0f) * 20.0f;
    }

    /* Temperature: ideal 18-26°C root zone */
    if (readings.soil_temp_c < 10.0f) {
        health -= 30.0f;
    } else if (readings.soil_temp_c < 18.0f) {
        health -= (18.0f - readings.soil_temp_c) * 2.0f;
    } else if (readings.soil_temp_c > 35.0f) {
        health -= 30.0f;
    }

    /* Light: PAR > 200 for most veggies */
    if (readings.par_umol_m2s < 50.0f) {
        health -= 40.0f;
    } else if (readings.par_umol_m2s < 200.0f) {
        health -= (200.0f - readings.par_umol_m2s) * 0.15f;
    }

    /* Leaf wetness: >6h wet = high fungal risk */
    if (readings.leaf_wet_duration_h > 6.0f) {
        health -= (readings.leaf_wet_duration_h - 6.0f) * 5.0f;
    }

    if (health > 100.0f) health = 100.0f;
    if (health < 0.0f) health = 0.0f;

    return (uint8_t)health;
}

/**
 * get_health_category - Classify health index
 */
static uint8_t get_health_category(uint8_t health)
{
    if (health >= HEALTH_THRIVING) return 0;   /* Thriving */
    if (health >= 60) return 1;                /* Good */
    if (health >= HEALTH_STRESSED) return 2;   /* Stressed */
    if (health >= HEALTH_CRITICAL) return 3;   /* Critical */
    return 4;                                   /* Dead */
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
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/**
 * mesh_send_soil_data - Send sensor readings to hub
 */
static void mesh_send_soil_data(void)
{
    urbanharvest_packet_t pkt;
    pkt.preamble[0] = 0xAA;
    pkt.preamble[1] = 0x55;
    pkt.src_id = node_id;
    pkt.dst_id = 0;  /* Hub */
    pkt.msg_type = 0x01;  /* SOIL_DATA */
    pkt.seq_num = mesh_seq++;

    /* Pack readings into payload */
    /* moisture(1) + ec(2) + temp(1) + par(2) + health(1) + leaf_wet(1) + battery(1) */
    pkt.payload[0] = (uint8_t)readings.soil_moisture_pct;  /* 0-100% */
    uint16_t ec_x10 = (uint16_t)(readings.soil_ec_ms_cm * 10.0f);
    pkt.payload[1] = (ec_x10 >> 8) & 0xFF;
    pkt.payload[2] = ec_x10 & 0xFF;
    pkt.payload[3] = (uint8_t)(readings.soil_temp_c + 40.0f);  /* Offset by 40 to fit -40 to +85 */
    uint16_t par_x10 = (uint16_t)(readings.par_umol_m2s * 10.0f);
    pkt.payload[4] = (par_x10 >> 8) & 0xFF;
    pkt.payload[5] = par_x10 & 0xFF;
    pkt.payload[6] = readings.health_index;
    pkt.payload[7] = (uint8_t)readings.leaf_wetness_pct;
    pkt.payload[8] = (uint8_t)readings.battery_voltage * 20;  /* ×20 to get 0-100 scale */
    pkt.payload[9] = readings.health_category;

    /* Leaf wetness hours (×10) */
    uint16_t wet_h_x10 = (uint16_t)(readings.leaf_wet_duration_h * 10.0f);
    pkt.payload[10] = (wet_h_x10 >> 8) & 0xFF;
    pkt.payload[11] = wet_h_x10 & 0xFF;

    pkt.len = sizeof(urbanharvest_packet_t);
    pkt.crc16 = crc16_ccitt((const uint8_t *)&pkt.src_id,
                             sizeof(urbanharvest_packet_t) - 4);

    /* TODO: Transmit via STM32WL55 integrated Sub-GHz radio */
    /* HAL_SUBGHZ_Init(); HAL_SUBGHZ_ExecSetCmd(); etc. */

    printk("TX: moisture=%.0f%% EC=%.1f temp=%.1f PAR=%.0f health=%d wet=%.1fh\n",
           readings.soil_moisture_pct, readings.soil_ec_ms_cm,
           readings.soil_temp_c, readings.par_umol_m2s,
           readings.health_index, readings.leaf_wet_duration_h);
}

/* ========== RGB LED STATUS ========== */

/**
 * update_status_led - Show health status on RGB LED
 * Green = thriving (80+), Yellow = good (60-80),
 * Orange = stressed (35-60), Red = critical (<35)
 */
static void update_status_led(void)
{
    /* TODO: WS2812B mini LED via GPIO bit-bang or SPI
     * Green:  (0, 255, 0)
     * Yellow: (255, 200, 0)
     * Orange: (255, 100, 0)
     * Red:    (255, 0, 0)
     */

    uint8_t h = readings.health_index;
    if (h >= 80) {
        printk("LED: GREEN (thriving)\n");
    } else if (h >= 60) {
        printk("LED: YELLOW (good)\n");
    } else if (h >= 35) {
        printk("LED: ORANGE (stressed)\n");
    } else {
        printk("LED: RED (critical)\n");
    }
}

/* ========== MAIN LOOP ========== */

void main(void)
{
    printk("UrbanHarvest Plant Sensor starting (node %d)...\n", node_id);

    /* Initialize sensors */
    /* TODO: I2C init for TSL25911 */
    /* TODO: OneWire init for DS18B20 */
    /* TODO: ADC init for moisture, EC, leaf wetness, VBAT */
    /* TODO: Sub-GHz radio init (STM32WL integrated) */

    /* Initialize readings to safe defaults */
    memset(&readings, 0, sizeof(readings));
    readings.soil_temp_c = 22.0f;
    readings.battery_voltage = 4.5f;

    uint32_t last_moisture_ms = 0;
    uint32_t last_ec_ms = 0;
    uint32_t last_temp_ms = 0;
    uint32_t last_light_ms = 0;
    uint32_t last_leaf_wet_ms = 0;
    uint32_t last_tx_ms = 0;

    printk("Sensor node ready — entering measurement loop\n");

    while (1) {
        uint32_t now = k_uptime_get_32();

        /* Soil moisture (every 5 min) */
        if ((now - last_moisture_ms) >= MOISTURE_INTERVAL_MS) {
            readings.soil_moisture_pct = read_soil_moisture();
            last_moisture_ms = now;
        }

        /* Soil EC (every 15 min) */
        if ((now - last_ec_ms) >= EC_INTERVAL_MS) {
            readings.soil_ec_ms_cm = read_soil_ec();
            last_ec_ms = now;
        }

        /* Soil temperature (every 15 min) */
        if ((now - last_temp_ms) >= TEMP_INTERVAL_MS) {
            readings.soil_temp_c = read_soil_temperature();
            last_temp_ms = now;
        }

        /* PAR light (every 10 min) */
        if ((now - last_light_ms) >= LIGHT_INTERVAL_MS) {
            readings.par_umol_m2s = read_par_light();
            last_light_ms = now;
        }

        /* Leaf wetness (every 5 min) */
        if ((now - last_leaf_wet_ms) >= LEAF_WET_INTERVAL_MS) {
            read_leaf_wetness();
            last_leaf_wet_ms = now;
        }

        /* Battery check (every hour, low priority) */
        if (now % 3600000 < 1000) {
            readings.battery_voltage = read_battery_voltage();
        }

        /* Calculate health index from all readings */
        readings.health_index = calculate_health_index();
        readings.health_category = get_health_category(readings.health_index);
        readings.timestamp_ms = now;

        /* Send to hub via mesh (every 5 min or immediately if urgent) */
        if ((now - last_tx_ms) >= MESH_TX_INTERVAL_MS ||
            readings.health_index < HEALTH_STRESSED ||
            readings.soil_moisture_pct < 15.0f) {

            mesh_send_soil_data();
            update_status_led();
            last_tx_ms = now;
        }

        /* Sleep until next measurement cycle */
        /* Ultra-low-power: RTC wakes us up, ~3µA sleep current */
        k_msleep(5000);  /* Check every 5 seconds for sensor timing */
    }
}