/**
 * BreathHome - Room Sensor Node Firmware
 * STM32WB55CG + SPS30 + SCD41 + SGP41 + SFA30 + BME688 + TSL25911 + SX1261
 * 
 * Multi-sensor air quality monitor. Reads all sensors, runs local
 * mold risk and AQI models, transmits to hub via Sub-GHz mesh.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/adc.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ========== CONSTANTS ========== */
#define SENSOR_READ_INTERVAL_MS   30000   /* 30 seconds */
#define CO2_READ_INTERVAL_MS      60000   /* 60 seconds */
#define RADON_AVG_INTERVAL_MS     3600000 /* 1 hour */
#define MESH_TX_SLOT_MS           50
#define MESH_FRAME_MS             900

#define I2C_SPS30_ADDR    0x69
#define I2C_SCD41_ADDR     0x62
#define I2C_SGP41_ADDR     0x59
#define I2C_SFA30_ADDR     0x6D
#define I2C_BME688_ADDR    0x76
#define I2C_TSL25911_ADDR  0x29

/* ========== DATA STRUCTURES ========== */

typedef struct __attribute__((packed)) {
    uint8_t  src_id;
    uint8_t  dst_id;
    uint8_t  msg_type;
    uint16_t seq_num;
    uint8_t  payload[48];
    uint16_t crc16;
} mesh_packet_t;

typedef struct {
    /* Particulate */
    float pm1_0;
    float pm2_5;
    float pm4_0;
    float pm10;
    /* Gases */
    float co2;
    float voc_index;
    float nox_index;
    float hcho;
    /* Environment */
    float temperature;
    float humidity;
    float pressure;
    uint16_t light_lux;
    /* Radon (basement nodes only) */
    float radon_bq_m3;
    /* Computed */
    float mold_risk_pct;
    uint16_t aqi_score;
    uint8_t  aqi_category;
} room_air_quality_t;

/* ========== GLOBALS ========== */
static room_air_quality_t air_data;
static uint8_t node_id = 0xFF;  /* Assigned by hub */
static uint8_t assigned_slot = 0xFF;
static uint16_t mesh_seq = 0;

/* Mold risk history buffer */
#define MOLD_HISTORY_HOURS 24
#define MOLD_HISTORY_SAMPLES (MOLD_HISTORY_HOURS * 4) /* 15-min samples */
static struct {
    float humidity;
    float voc_index;
    float temperature;
    float dew_point;
} mold_history[MOLD_HISTORY_SAMPLES];
static int mold_history_idx = 0;

/* ========== I2C SENSORS ========== */

static const struct device *i2c_dev;

/**
 * i2c_write_cmd - Write command to I2C sensor
 */
static int i2c_write_cmd(uint8_t addr, const uint8_t *cmd, uint16_t len)
{
    return i2c_write(i2c_dev, cmd, len, addr);
}

/**
 * i2c_read_data - Read data from I2C sensor
 */
static int i2c_read_data(uint8_t addr, uint8_t *data, uint16_t len)
{
    return i2c_read(i2c_dev, data, len, addr);
}

/**
 * sps30_init - Initialize Sensirion SPS30 particulate sensor
 */
static int sps30_init(void)
{
    uint8_t cmd[2] = {0x21, 0x03};  /* Start measurement, PM mode */
    int ret = i2c_write_cmd(I2C_SPS30_ADDR, cmd, 2);
    if (ret != 0) {
        printk("SPS30 init failed: %d\n", ret);
        return ret;
    }
    k_msleep(100);
    
    /* Set fan to low-power mode: 10s on, 30s off */
    uint8_t clean_interval[5] = {0x80, 0x00, 0x00, 0x00, 0x00};
    /* Auto-cleaning interval: 604800 seconds (7 days) */
    clean_interval[2] = 0x09;
    clean_interval[3] = 0x3F;
    clean_interval[4] = 0x00;
    
    return 0;
}

/**
 * sps30_read - Read particulate matter data from SPS30
 */
static int sps30_read(void)
{
    uint8_t cmd[2] = {0x21, 0x02};  /* Read measured values */
    uint8_t data[40];
    
    int ret = i2c_write_cmd(I2C_SPS30_ADDR, cmd, 2);
    if (ret != 0) return ret;
    k_msleep(50);
    
    ret = i2c_read_data(I2C_SPS30_ADDR, data, 40);
    if (ret != 0) return ret;
    
    /* SPS30 data: 10 values × 4 bytes (MSB, MSB-CRC, LSB, LSB-CRC) */
    /* Each value is IEEE 754 float in big-endian */
    uint32_t pm1_0_raw = (data[0] << 24) | (data[1] << 16) | (data[3] << 8) | data[4];
    uint32_t pm2_5_raw = (data[5] << 24) | (data[6] << 16) | (data[8] << 8) | data[9];
    uint32_t pm4_0_raw = (data[10] << 24) | (data[11] << 16) | (data[13] << 8) | data[14];
    uint32_t pm10_raw  = (data[15] << 24) | (data[16] << 16) | (data[18] << 8) | data[19];
    
    air_data.pm1_0 = *(float *)&pm1_0_raw;
    air_data.pm2_5 = *(float *)&pm2_5_raw;
    air_data.pm4_0 = *(float *)&pm4_0_raw;
    air_data.pm10  = *(float *)&pm10_raw;
    
    return 0;
}

/**
 * scd41_init - Initialize Sensirion SCD41 CO2 sensor
 */
static int scd41_init(void)
{
    uint8_t cmd[2] = {0x21, 0xB1};  /* Start periodic measurement */
    int ret = i2c_write_cmd(I2C_SCD41_ADDR, cmd, 2);
    if (ret != 0) {
        printk("SCD41 init failed: %d\n", ret);
    }
    return ret;
}

/**
 * scd41_read - Read CO2, temperature, humidity from SCD41
 */
static int scd41_read(void)
{
    uint8_t cmd[2] = {0xE4, 0xB8};  /* Read measurement */
    uint8_t data[9];
    
    int ret = i2c_write_cmd(I2C_SCD41_ADDR, cmd, 2);
    if (ret != 0) return ret;
    k_msleep(10);
    
    ret = i2c_read_data(I2C_SCD41_ADDR, data, 9);
    if (ret != 0) return ret;
    
    /* CO2: word 0 */
    uint16_t co2_word = (data[0] << 8) | data[1];
    air_data.co2 = (float)co2_word;
    
    /* Temperature: word 1 (scaled: T = -45 + 175 * word/2^16) */
    uint16_t temp_word = (data[3] << 8) | data[4];
    air_data.temperature = -45.0f + 175.0f * (float)temp_word / 65536.0f;
    
    /* Humidity: word 2 (scaled: RH = 100 * word/2^16) */
    uint16_t rh_word = (data[6] << 8) | data[7];
    air_data.humidity = 100.0f * (float)rh_word / 65536.0f;
    
    return 0;
}

/**
 * sgp41_init - Initialize Sensirion SGP41 VOC/NOx sensor
 */
static int sgp41_init(void)
{
    /* SGP41 requires conditioning: run without RH compensation for first 64 readings */
    uint8_t cmd[2] = {0x26, 0x21};  /* Measure raw signals */
    int ret = i2c_write_cmd(I2C_SGP41_ADDR, cmd, 2);
    if (ret != 0) {
        printk("SGP41 init failed: %d\n", ret);
    }
    return ret;
}

/**
 * sgp41_read - Read VOC and NOx raw signals from SGP41
 */
static int sgp41_read(void)
{
    /* SGP41: Measure raw with humidity compensation */
    /* Condition: 0x26, 0x21 then after 64 readings: 0x26, 0x10 */
    uint16_t default_rh = 0x8000;  /* 50% RH */
    uint16_t default_t = 0x6666;   /* 25°C */
    uint8_t cmd[8] = {
        0x26, 0x10,  /* Measure raw signals with humidity compensation */
        (default_rh >> 8) & 0xFF, default_rh & 0xFF,
        0x00,  /* CRC placeholder */
        (default_t >> 8) & 0xFF, default_t & 0xFF,
        0x00   /* CRC placeholder */
    };
    /* CRC calculation omitted for brevity */
    
    uint8_t data[6];
    int ret = i2c_write_cmd(I2C_SGP41_ADDR, cmd, 8);
    if (ret != 0) return ret;
    k_msleep(50);  /* Measurement takes ~40ms */
    
    ret = i2c_read_data(I2C_SGP41_ADDR, data, 6);
    if (ret != 0) return ret;
    
    /* Raw VOC signal (SRAW_VOC) */
    uint16_t raw_voc = (data[0] << 8) | data[1];
    /* Raw NOx signal (SRAW_NOX) */
    uint16_t raw_nox = (data[3] << 8) | data[4];
    
    /* Convert to VOC index using Sensirion algorithm */
    /* Simplified: VOC_index = raw_voc / initial_baseline * 100 */
    /* In production, use Sensirion's VOC algorithm library */
    air_data.voc_index = (float)raw_voc / 3.0f;  /* Simplified scaling */
    air_data.nox_index = (float)raw_nox / 3.0f;
    
    return 0;
}

/**
 * sfa30_init - Initialize Sensirion SFA30 formaldehyde sensor
 */
static int sfa30_init(void)
{
    uint8_t cmd[2] = {0x24, 0x00};  /* Start continuous measurement */
    int ret = i2c_write_cmd(I2C_SFA30_ADDR, cmd, 2);
    if (ret != 0) {
        printk("SFA30 init failed: %d\n", ret);
    }
    return ret;
}

/**
 * sfa30_read - Read formaldehyde from SFA30
 */
static int sfa30_read(void)
{
    uint8_t cmd[2] = {0x04, 0x43};  /* Read measurement */
    uint8_t data[9];
    
    int ret = i2c_write_cmd(I2C_SFA30_ADDR, cmd, 2);
    if (ret != 0) return ret;
    k_msleep(10);
    
    ret = i2c_read_data(I2C_SFA30_ADDR, data, 9);
    if (ret != 0) return ret;
    
    /* HCHO in ppb, then convert to ppm */
    uint16_t hcho_ppb = (data[0] << 8) | data[1];
    air_data.hcho = (float)hcho_ppb / 1000.0f;  /* ppb to ppm */
    
    return 0;
}

/**
 * bme688_read - Read temperature, humidity, pressure, gas resistance from BME688
 */
static int bme688_read(void)
{
    uint8_t data[8];
    /* BME688: forced mode measurement */
    /* In production: use Bosch BME688 API for proper oversampling and gas heater */
    
    /* Simplified: read all registers */
    uint8_t reg = 0x1D;  /* Press MSB */
    int ret = i2c_write_cmd(I2C_BME688_ADDR, &reg, 1);
    if (ret != 0) return ret;
    k_msleep(50);
    ret = i2c_read_data(I2C_BME688_ADDR, data, 8);
    if (ret != 0) return ret;
    
    /* Use SCD41 for more accurate temperature and humidity */
    /* BME688 provides pressure and gas resistance for IAQ */
    
    /* Pressure: registers 1D-20 */
    int32_t press_raw = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4);
    air_data.pressure = (float)press_raw / 100.0f;  /* Simplified conversion */
    
    return 0;
}

/**
 * tsl25911_read - Read light level from TSL25911
 */
static int tsl25911_read(void)
{
    uint8_t cmd[2] = {0xA0, 0x14};  /* Command bit + CDATA register */
    int ret = i2c_write_cmd(I2C_TSL25911_ADDR, cmd, 2);
    if (ret != 0) return ret;
    k_msleep(5);
    
    uint8_t data[4];
    ret = i2c_read_data(I2C_TSL25911_ADDR, data, 4);
    if (ret != 0) return ret;
    
    /* Channel 0 (visible + IR) and Channel 1 (IR only) */
    uint16_t ch0 = (data[1] << 8) | data[0];
    uint16_t ch1 = (data[3] << 8) | data[2];
    
    /* Lux calculation: simplified TSL25911 formula */
    float lux;
    if (ch0 == ch1) {
        lux = 0.0f;
    } else {
        float ratio = (float)ch1 / (float)ch0;
        lux = (ch0 - ch1) * (1.0f - ratio) * 40.0f;  /* Simplified */
    }
    air_data.light_lux = (uint16_t)(lux > 65535 ? 65535 : lux);
    
    return 0;
}

/* ========== MOLD RISK CALCULATION ========== */

/**
 * calculate_mold_risk - Estimate mold growth risk based on environmental history
 * 
 * Mold growth requires:
 * - Relative humidity > 60% (optimal > 80%)
 * - Temperature 10-35°C (optimal 25-30°C)
 * - Time: 24-48 hours at favorable conditions
 * - VOC changes from mold off-gassing
 * 
 * This simplified model computes risk based on:
 * - Current humidity and temperature
 * - Hours of elevated humidity in last 24h
 * - VOC trend (rising VOC index suggests biological activity)
 */
static float calculate_mold_risk(void)
{
    float risk = 0.0f;
    
    /* Current humidity factor */
    if (air_data.humidity > 80.0f) {
        risk += 40.0f;
    } else if (air_data.humidity > 70.0f) {
        risk += 30.0f * (air_data.humidity - 70.0f) / 10.0f;
    } else if (air_data.humidity > 60.0f) {
        risk += 20.0f * (air_data.humidity - 60.0f) / 10.0f;
    }
    
    /* Temperature factor (mold grows 10-35°C, optimal 25-30°C) */
    float temp_factor = 0.0f;
    if (air_data.temperature >= 10.0f && air_data.temperature <= 35.0f) {
        float optimal_dist = fabsf(air_data.temperature - 27.5f);
        temp_factor = 30.0f * (1.0f - optimal_dist / 17.5f);
        if (temp_factor < 0) temp_factor = 0;
    }
    risk += temp_factor;
    
    /* Wet surface hours: count hours where humidity > 70% in last 24h */
    float wet_hours = 0.0f;
    for (int i = 0; i < MOLD_HISTORY_SAMPLES; i++) {
        if (mold_history[i].humidity > 70.0f) {
            wet_hours += 0.25f;  /* Each sample is 15 minutes */
        }
    }
    risk += 20.0f * (wet_hours / 24.0f);  /* Scale: full day wet = +20% */
    
    /* VOC trend: rising VOC index suggests biological activity */
    if (air_data.voc_index > 200.0f) {
        risk += 10.0f * (air_data.voc_index - 200.0f) / 100.0f;
    }
    
    /* Clamp to 0-100 */
    if (risk > 100.0f) risk = 100.0f;
    if (risk < 0.0f) risk = 0.0f;
    
    return risk;
}

/**
 * update_mold_history - Add current reading to mold history ring buffer
 */
static void update_mold_history(void)
{
    /* Calculate dew point */
    float a = 17.27f;
    float b = 237.7f;
    float alpha = (a * air_data.temperature) / (b + air_data.temperature) + logf(air_data.humidity / 100.0f);
    float dew_point = (b * alpha) / (a - alpha);
    
    mold_history[mold_history_idx].humidity = air_data.humidity;
    mold_history[mold_history_idx].voc_index = air_data.voc_index;
    mold_history[mold_history_idx].temperature = air_data.temperature;
    mold_history[mold_history_idx].dew_point = dew_point;
    
    mold_history_idx = (mold_history_idx + 1) % MOLD_HISTORY_SAMPLES;
}

/* ========== AQI CALCULATION (LOCAL) ========== */

/**
 * calculate_aqi - Compute composite AQI from sensor readings
 * Simplified version matching hub algorithm for local alert decisions
 */
static uint16_t calculate_aqi(void)
{
    float composite = 0.0f;
    
    /* PM2.5 sub-index */
    if (air_data.pm2_5 <= 12.0f) {
        composite = 50.0f * air_data.pm2_5 / 12.0f;
    } else if (air_data.pm2_5 <= 35.4f) {
        composite = 50.0f + 50.0f * (air_data.pm2_5 - 12.1f) / 23.3f;
    } else if (air_data.pm2_5 <= 55.4f) {
        composite = 100.0f + 50.0f * (air_data.pm2_5 - 35.5f) / 19.9f;
    } else {
        composite = 150.0f + 50.0f * (air_data.pm2_5 - 55.5f) / 94.9f;
    }
    
    /* CO2 sub-index */
    float co2_idx;
    if (air_data.co2 <= 800.0f) {
        co2_idx = 50.0f * air_data.co2 / 800.0f;
    } else if (air_data.co2 <= 1200.0f) {
        co2_idx = 50.0f + 50.0f * (air_data.co2 - 800.0f) / 400.0f;
    } else if (air_data.co2 <= 1800.0f) {
        co2_idx = 100.0f + 50.0f * (air_data.co2 - 1200.0f) / 600.0f;
    } else {
        co2_idx = 150.0f + 50.0f * (air_data.co2 - 1800.0f) / 700.0f;
    }
    if (co2_idx > composite) composite = co2_idx;
    
    /* VOC sub-index */
    float voc_idx;
    if (air_data.voc_index <= 100.0f) {
        voc_idx = 50.0f * air_data.voc_index / 100.0f;
    } else if (air_data.voc_index <= 200.0f) {
        voc_idx = 50.0f + 50.0f * (air_data.voc_index - 100.0f) / 100.0f;
    } else {
        voc_idx = 100.0f + 50.0f * (air_data.voc_index - 200.0f) / 100.0f;
    }
    if (voc_idx > composite) composite = voc_idx;
    
    /* Formaldehyde penalty */
    if (air_data.hcho > 0.08f) {
        composite += 20.0f * (air_data.hcho / 0.08f);
    }
    
    /* Mold risk penalty */
    if (air_data.mold_risk_pct > 60.0f) {
        composite += 15.0f * (air_data.mold_risk_pct / 60.0f);
    }
    
    if (composite > 500.0f) composite = 500.0f;
    if (composite < 0.0f) composite = 0.0f;
    
    return (uint16_t)composite;
}

/* ========== SX1261 SUB-GHZ RADIO ========== */

/* SX1261 driver - similar to hub's SX1262 but with lower TX power (+15dBm max) */
/* Using same SPI/command interface, simplified here */

static const struct device *spi1_dev;
static struct gpio_dt_spec sx1261_nss = GPIO_DT_SPEC_GET(DT_NODELABEL(sx1261_nss), gpios);
static struct gpio_dt_spec sx1261_busy = GPIO_DT_SPEC_GET(DT_NODELABEL(sx1261_busy), gpios);
static struct gpio_dt_spec sx1261_irq = GPIO_DT_SPEC_GET(DT_NODELABEL(sx1261_irq), gpios);
static struct gpio_dt_spec sx1261_nrst = GPIO_DT_SPEC_GET(DT_NODELABEL(sx1261_nrst), gpios);

/**
 * sx1261_init - Initialize SX1261 radio for Sub-GHz mesh
 */
static int sx1261_init(void)
{
    /* Reset */
    gpio_pin_set_dt(&sx1261_nrst, 0);
    k_msleep(10);
    gpio_pin_set_dt(&sx1261_nrst, 1);
    k_msleep(50);
    
    /* Set to standby RC */
    uint8_t standby[] = { 0x00 };
    /* Write command 0x80 (SetStandby) with param 0x00 (RC osc) */
    
    /* Set packet type to LoRa */
    /* Configure SF7, 125kHz BW, 868MHz */
    /* Set TX power to +14dBm */
    /* Set sync word to BreathHome */
    
    return 0;
}

/**
 * sx1261_transmit_packet - Send mesh packet in assigned TDMA slot
 */
static int sx1261_transmit_packet(const mesh_packet_t *pkt)
{
    /* Build packet buffer */
    /* Send via SX1261 SPI */
    /* Wait for TX done interrupt */
    return 0;
}

/* ========== MESH PROTOCOL ========== */

/**
 * mesh_send_air_quality - Package and send air quality data to hub
 */
static int mesh_send_air_quality(void)
{
    mesh_packet_t pkt;
    pkt.src_id = node_id;
    pkt.dst_id = 0;  /* Hub */
    pkt.msg_type = 0x01;  /* AIR_QUALITY */
    pkt.seq_num = mesh_seq++;
    
    /* Pack air quality data into payload (48 bytes) */
    /* Format: PM2.5(f32) + PM10(f32) + CO2(f32) + VOC(f32) + HCHO(f32) +
     *          TEMP(f32) + RH(f32) + PRESS(f32) + AQI(u16) + CATEGORY(u8) +
     *          MOLD_RISK(f32) + LIGHT(u16) + RADON(f32) = 47 bytes */
    uint8_t *p = pkt.payload;
    
    memcpy(p, &air_data.pm2_5, 4); p += 4;
    memcpy(p, &air_data.pm10, 4); p += 4;
    memcpy(p, &air_data.co2, 4); p += 4;
    memcpy(p, &air_data.voc_index, 4); p += 4;
    memcpy(p, &air_data.hcho, 4); p += 4;
    memcpy(p, &air_data.temperature, 4); p += 4;
    memcpy(p, &air_data.humidity, 4); p += 4;
    memcpy(p, &air_data.pressure, 4); p += 4;
    memcpy(p, &air_data.aqi_score, 2); p += 2;
    *p++ = air_data.aqi_category;
    memcpy(p, &air_data.mold_risk_pct, 4); p += 4;
    memcpy(p, &air_data.light_lux, 2); p += 2;
    memcpy(p, &air_data.radon_bq_m3, 4); p += 4;
    
    return sx1261_transmit_packet(&pkt);
}

/**
 * mesh_send_danger_alert - Send critical alert (bypasses TDMA)
 */
static int mesh_send_danger_alert(uint8_t alert_type, float value)
{
    mesh_packet_t pkt;
    pkt.src_id = node_id;
    pkt.dst_id = 0;  /* Hub */
    pkt.msg_type = 0x09;  /* DANGER_ALERT */
    pkt.seq_num = mesh_seq++;
    
    pkt.payload[0] = alert_type;  /* 0=PM25, 1=CO2, 2=VOC, 3=HCHO, 4=RADON, 5=MOLD */
    memcpy(&pkt.payload[1], &value, 4);
    pkt.payload[5] = air_data.aqi_category;
    
    return sx1261_transmit_packet(&pkt);
}

/* ========== SENSOR THREAD ========== */

static void sensor_read_thread(void *p1, void *p2, void *p3)
{
    int ret;
    int scd41_ready = 0;
    int sgp41_conditioning = 0;
    
    /* Initialize all sensors */
    sps30_init();
    scd41_init();
    sgp41_init();
    sfa30_init();
    
    k_msleep(1000);  /* Wait for sensors to stabilize */
    
    while (1) {
        /* Read particulate (every 30s) */
        ret = sps30_read();
        if (ret != 0) {
            printk("SPS30 read error: %d\n", ret);
        }
        
        /* Read CO2 + temp + humidity from SCD41 (every 60s, needs 5s warmup) */
        scd41_ready++;
        if (scd41_ready >= 2) {  /* Every 60s at 30s intervals */
            ret = scd41_read();
            if (ret != 0) {
                printk("SCD41 read error: %d\n", ret);
            }
            scd41_ready = 0;
        }
        
        /* Read VOC/NOx (every 30s, after conditioning period) */
        sgp41_conditioning++;
        if (sgp41_conditioning > 64) {
            ret = sgp41_read();
            if (ret != 0) {
                printk("SGP41 read error: %d\n", ret);
            }
        }
        
        /* Read formaldehyde (every 30s) */
        ret = sfa30_read();
        if (ret != 0) {
            printk("SFA30 read error: %d\n", ret);
        }
        
        /* Read BME688 pressure (temperature/humidity from SCD41 is more accurate) */
        bme688_read();
        
        /* Read light level */
        tsl25911_read();
        
        /* Update mold history */
        update_mold_history();
        
        /* Calculate mold risk */
        air_data.mold_risk_pct = calculate_mold_risk();
        
        /* Calculate AQI */
        air_data.aqi_score = calculate_aqi();
        if (air_data.aqi_score <= 50) air_data.aqi_category = 0;
        else if (air_data.aqi_score <= 100) air_data.aqi_category = 1;
        else if (air_data.aqi_score <= 150) air_data.aqi_category = 2;
        else if (air_data.aqi_score <= 200) air_data.aqi_category = 3;
        else if (air_data.aqi_score <= 300) air_data.aqi_category = 4;
        else air_data.aqi_category = 5;
        
        /* Check for danger alerts */
        if (air_data.pm2_5 > 150.0f) {
            mesh_send_danger_alert(0, air_data.pm2_5);
        }
        if (air_data.co2 > 2500.0f) {
            mesh_send_danger_alert(1, air_data.co2);
        }
        if (air_data.radon_bq_m3 > 148.0f) {
            mesh_send_danger_alert(4, air_data.radon_bq_m3);
        }
        
        k_msleep(SENSOR_READ_INTERVAL_MS);
    }
}

/* ========== MESH TX THREAD ========== */

static void mesh_tx_thread(void *p1, void *p2, void *p3)
{
    while (1) {
        /* Wait for assigned TDMA slot */
        if (assigned_slot == 0xFF) {
            /* Not yet registered with hub, advertise for pairing */
            k_msleep(5000);
            continue;
        }
        
        /* Calculate slot timing */
        int64_t frame_start = k_uptime_get() % MESH_FRAME_MS;
        int64_t slot_time = assigned_slot * MESH_TX_SLOT_MS;
        int64_t wait_time = slot_time - frame_start;
        
        if (wait_time > 0) {
            k_msleep(wait_time);
        }
        
        /* Transmit air quality data */
        mesh_send_air_quality();
        
        /* Sleep until next frame */
        k_msleep(MESH_FRAME_MS - MESH_TX_SLOT_MS);
    }
}

K_THREAD_DEFINE(sensor_tid, 2048, sensor_read_thread, NULL, NULL, NULL, 5, 0, 0);
K_THREAD_DEFINE(mesh_tid, 1024, mesh_tx_thread, NULL, NULL, NULL, 4, 0, 0);

int main(void)
{
    /* Initialize I2C */
    i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
    
    /* Initialize GPIOs */
    gpio_pin_configure_dt(&sx1261_nss, GPIO_OUTPUT_HIGH);
    gpio_pin_configure_dt(&sx1261_busy, GPIO_INPUT);
    gpio_pin_configure_dt(&sx1261_irq, GPIO_INPUT);
    gpio_pin_configure_dt(&sx1261_nrst, GPIO_OUTPUT_HIGH);
    
    /* Initialize SPI */
    spi1_dev = DEVICE_DT_GET(DT_NODELABEL(spi1));
    
    /* Initialize SX1261 */
    sx1261_init();
    
    /* Initialize sensors */
    /* Done in sensor_read_thread */
    
    printk("BreathHome Room Sensor started\n");
    printk("Waiting for hub pairing...\n");
    
    return 0;
}